/**
 * WISE_Processing_Lib: kmlinternal.cpp
 * Copyright (C) 2023  WISE
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "kmlinternal.h"

#include "WTime.h"

#include <cctype>
#include <codecvt>
#include <mutex>
#include <errno.h>
#include <minizip/unzip.h>
#include <minizip/zip.h>

#include <boost/algorithm/string.hpp>

using namespace KML::Internal;
using namespace KML::Internal::Input;
using namespace KML::Internal::Output;
using namespace xercesc;
using namespace HSS_Time;
using namespace std;

bool iequals(const xerces_string &str1, const xerces_string &str2)
{
	return (str1.size() == str2.size()) && std::equal(str1.begin(), str1.end(), str2.begin(), [](const xerces_char& c1, const xerces_char& c2)
	{
		return (c1 == c2) || (std::toupper(c1) == std::toupper(c2));
	});
}


#ifdef _MSC_VER
#define _CHAR_ int16_t
#else
#define _CHAR_ xerces_char
#endif


std::string KML::Internal::utf16_to_utf8(const xerces_string &utf16_string)
{
    std::wstring_convert<std::codecvt_utf8_utf16<_CHAR_>, _CHAR_> convert;
    auto p = reinterpret_cast<const _CHAR_ *>(utf16_string.data());
    return convert.to_bytes(p, p + utf16_string.size());
}

xerces_string KML::Internal::utf8_to_utf16(const std::string &utf8_string)
{
#ifdef XERCES_USE_U
	std::wstring_convert<std::codecvt_utf8_utf16<_CHAR_>, _CHAR_> convert;
	auto p = convert.from_bytes(utf8_string);
	xerces_string utf16_string(reinterpret_cast<const xerces_char *>(p.data()));
	return utf16_string;
#else
	std::wstring_convert<std::codecvt_utf8_utf16<xerces_char>> convert;
	return convert.from_bytes(utf8_string);
#endif
}


int KML::Internal::stoi(const xerces_string& _Str, size_t *_Idx, int _Base) {	// convert wstring to double
	std::string str = utf16_to_utf8(_Str);
	const char *_Ptr = str.c_str();
	char *_Eptr;
	_set_errno(0);
	long _Ans = strtol(_Ptr, &_Eptr, _Base);

	if (_Ptr == _Eptr)
		throw std::invalid_argument("invalid stoi argument");
	if ((*_errno()) == ERANGE || _Ans < INT_MIN || INT_MAX < _Ans)
		throw std::out_of_range("stoi argument out of range");
	if (_Idx != 0)
		*_Idx = (size_t)(_Eptr - _Ptr);
	return ((int)_Ans);
}


double KML::Internal::stod(const xerces_string& _Str, size_t *_Idx) {	// convert wstring to double
	std::string str = utf16_to_utf8(_Str);
	const char *_Ptr = str.c_str();
	char *_Eptr;
	_set_errno(0);
	double _Ans = strtod(_Ptr, &_Eptr);

	if (_Ptr == _Eptr)
		throw std::invalid_argument("invalid stod argument");
	if ((*_errno()) == ERANGE)
		throw std::out_of_range("stod argument out of range");
	if (_Idx != 0)
		*_Idx = (size_t)(_Eptr - _Ptr);
	return (_Ans);
}


std::mutex s_mutex;
std::uint32_t s_counter{ 0 };

void initializeXML()
{
	std::lock_guard<std::mutex> lock(s_mutex);
	if (s_counter == 0)
		XMLPlatformUtils::Initialize();
	s_counter++;
}

void deinitializeXML()
{
	std::lock_guard<std::mutex> lock(s_mutex);
	s_counter--;
	if (s_counter == 0)
		XMLPlatformUtils::Terminate();
}


inline
#ifdef XERCES_USE_U
std::u16string
#else
std::string
#endif
pathToString(const kmlFs::path& path)
{
#ifdef XERCES_USE_U
	return path.u16string();
#else
	return path.string();
#endif
}


constexpr std::size_t BUFFER_SIZE = 4096;

std::vector<unsigned char> extractFile(const kmlFs::path& input, const std::string& fileToExtract)
{
	if (!fs::exists(input))
		return {};

	//buffers for extraction
	char currentFilename[512];
	void* buffer = operator new(BUFFER_SIZE);
	unz_file_info info;

	//open the archive
	auto mcontext_ = unzOpen(input.string().c_str());
	//open the first file in the archive
	std::int32_t error = unzGoToFirstFile(mcontext_);
	std::vector<unsigned char> data;

	while (error == UNZ_OK)
	{
		//get the name of the current file
		unzGetCurrentFileInfo(mcontext_,
			&info, currentFilename, sizeof(currentFilename), 0, 0, 0, 0);

		//create the output filename
		std::string name = std::string(currentFilename);

		//we have found the file we want to exract
		if (boost::iequals(fileToExtract, name))
		{
			unzOpenCurrentFile(mcontext_);

			std::int32_t readCount = 0;
			std::int32_t totalRead = 0;
			do
			{
				//not sure this is the most efficient way to do this
				data.resize(totalRead + BUFFER_SIZE);
				readCount = unzReadCurrentFile(mcontext_, (char*)(&data[totalRead]), BUFFER_SIZE);
				totalRead += readCount;
				if (readCount < BUFFER_SIZE)
				{
					data.resize(totalRead);
					break;
				}
			} while (readCount > 0);

			unzCloseCurrentFile(mcontext_);
			break;
		}

		//go to the next file in the archive
		error = unzGoToNextFile(mcontext_);
	}

	unzClose(mcontext_);
	operator delete(buffer);

	return data;
}


bool createZipFile(const kmlFs::path& zipPath, const std::string& filename, const XMLByte* data, const XMLSize_t dataLength)
{
	//open the archive for writing
	auto mcontext_ = zipOpen64(zipPath.string().c_str(), APPEND_STATUS_CREATE);

	//set the last modified time
	zip_fileinfo zi = { 0 };
	time_t rawTime;
	time(&rawTime);

#ifdef _MSC_VER
	struct tm gmt;
	gmtime_s(&gmt, &rawTime);
#else
	struct tm gmt;
	gmtime_r(&rawTime, &gmt);
#endif

	zi.tmz_date.tm_sec = gmt.tm_sec;
	zi.tmz_date.tm_min = gmt.tm_min;
	zi.tmz_date.tm_hour = gmt.tm_hour;
	zi.tmz_date.tm_mday = gmt.tm_mday;
	zi.tmz_date.tm_mon = gmt.tm_mon;
	zi.tmz_date.tm_year = gmt.tm_year;

#ifdef Z_DEFLATED
	auto err = zipOpenNewFileInZip64(mcontext_, filename.c_str(),
		&zi, nullptr, 0, nullptr, 0, nullptr, Z_DEFLATED, 9, dataLength > 0xffffffff);
#else
	auto err = zipOpenNewFileInZip(mcontext_, archiveName.c_str(),
		&zi, nullptr, 0, nullptr, 0, nullptr, Z_BZIP2ED, 9);
#endif

	if (err == ZIP_OK)
	{
		err = zipWriteInFileInZip(mcontext_, data, dataLength);

		//close the input and output files
		zipCloseFileInZip(mcontext_);
	}

	//close the archive
	zipClose(mcontext_, nullptr);

	return err == ZIP_OK;
}


xercesc::DOMNode* findNode(xercesc::DOMNode* parent, const xerces_string& name)
{
	auto child = parent->getFirstChild();
	while (child)
	{
		if (iequals(name, child->getNodeName()))
			return child;
		child = child->getNextSibling();
	}

	return nullptr;
}


void Java::Internal::read_job_directory(const kmlFs::path& path, std::string& job_directory)
{
	if (kmlFs::exists(path))
	{
		XercesDOMParser mParser;
		mParser.setValidationScheme(XercesDOMParser::Val_Never);
		mParser.setDoNamespaces(false);
		mParser.setDoSchema(false);
		mParser.setLoadExternalDTD(false);
#ifdef XERCES_USE_U
		std::u16string str = path.u16string();
#else
		std::string str = path.string();
#endif
		mParser.parse(str.c_str());

		xercesc::DOMDocument* dom = mParser.getDocument();
		xercesc::DOMElement* map = dom->getDocumentElement();

		xercesc::DOMNode* n1 = map->getFirstChild();
		while (n1 != nullptr)
		{
			if (iequals(n1->getNodeName(), _X("entry")))
			{
				xercesc::DOMElement* el = dynamic_cast<xercesc::DOMElement*>(n1);
				if (el) {
					xerces_string key = el->getAttribute(_X("key"));
					if (iequals(key, _X("job_directory")))
					{
						xerces_string value = el->getAttribute(_X("value"));
#ifdef XERCES_USE_U
						job_directory = utf16_to_utf8(value);
#else
						job_directory = value;
#endif
						break;
					}
				}
			}

			n1 = n1->getNextSibling();
		}
	}
}


KML::Internal::Input::InputKmlFile::InputKmlFile(kmlFs::path input)
	: document(nullptr)
{
	initialize(input, "doc.kml");
}

KML::Internal::Input::InputKmlFile::~InputKmlFile()
{
	if (document)
		delete document;
}

bool KML::Internal::Input::InputKmlFile::initialize(const kmlFs::path& input, const std::string& kmzPath)
{
	if (kmlFs::exists(input))
	{
		XercesDOMParser mParser;
		mParser.setValidationScheme(XercesDOMParser::Val_Never);
		mParser.setDoNamespaces(false);
		mParser.setDoSchema(false);
		mParser.setLoadExternalDTD(false);
#ifdef XERCES_USE_U
		std::u16string str = input.u16string();
#else
		std::string str = input.string();
#endif

		if (boost::iequals(input.extension().string(), ".kmz"))
		{
			auto fileData = extractFile(input, kmzPath);
			if (fileData.size())
			{
				MemBufInputSource buf(&fileData[0], fileData.size(), (pathToString(input.filename()) + _X(" (in memory)")).c_str());
				mParser.parse(buf);
			}
			else
				throw kmlFs::filesystem_error("Invalid KMZ file", input, std::error_code());
		}
		else
			mParser.parse(str.c_str());

		xercesc::DOMDocument* dom = mParser.getDocument();
		xercesc::DOMElement* kml = dom->getDocumentElement();

		ns = kml->getAttribute(_X("xmlns"));

		xercesc::DOMNode* n1 = kml->getFirstChild();
		while (n1 != nullptr)
		{
			if (iequals(n1->getNodeName(), _X("Document")))
			{
				document = new InputDocument(n1);

				if (document->link.size() > 0)
				{
					auto filename = document->link;
					delete document;
					document = nullptr;
					ns.clear();

					//skip the rest of the method
					return initialize(input, utf16_to_utf8(filename));
				}
				break;
			}

			n1 = n1->getNextSibling();
		}
		if (document && document->folder)
		{
			for (int i = 0; i < document->folder->placemark.size() - 1; i++)
				document->folder->placemark[i]->_next = document->folder->placemark[i + 1];
		}
	}

	return document != nullptr;
}

bool KML::Internal::Input::InputKmlFile::save(kmlFs::path output)
{
	bool isKmz = boost::iequals(output.extension().string(), ".kmz");
	xercesc::DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation(_X("Core"));
	if (impl != nullptr)
	{
		xercesc::DOMDocument* doc = impl->createDocument(0, _X("kml"), 0);
		xercesc::DOMElement* kml = doc->getDocumentElement();

		if (document)
			document->save(doc, kml);

		xercesc::DOMImplementation *implementation = DOMImplementationRegistry::getDOMImplementation(_X("LS"));
		// Create a DOMLSSerializer which is used to serialize a DOM tree into an XML document
		xercesc::DOMLSSerializer *serializer = ((DOMImplementationLS*)implementation)->createLSSerializer();
		//pretty print the exported xml
		if (serializer->getDomConfig()->canSetParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true))
			serializer->getDomConfig()->setParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true);
		XMLFormatTarget* formatTarget;
		// Specify the target for the XML output
		if (isKmz)
		{
			//for KMZ build the output file in memory
			formatTarget = new MemBufFormatTarget();
		}
		else
		{
#ifdef XERCES_USE_U
			formatTarget = new LocalFileFormatTarget(output.u16string().c_str());
#else
			formatTarget = new LocalFileFormatTarget(output.c_str());
#endif
		}
		// Create a new empty output destination object
		xercesc::DOMLSOutput *domout = ((DOMImplementationLS*)implementation)->createLSOutput();
		// Set the stream to our target
		domout->setByteStream(formatTarget);
		// Write the serialized output to the destination
		serializer->write(doc, domout);

		//write the KMZ file
		if (isKmz)
		{
			auto target = static_cast<MemBufFormatTarget*>(formatTarget);
			createZipFile(output, "doc.kml", target->getRawBuffer(), target->getLen());
		}

		serializer->release();
		delete formatTarget;
		domout->release();

		doc->release();
		
		return true;
	}
	
	return false;
}

KML::Internal::Output::OutputKmlFile::OutputKmlFile(const KML::Internal::Input::InputKmlFile* input, const HSS_Time::WTimeSpan& offset)
{
	ns = input->ns;
	if (input->document)
		document = new OutputDocument(input->document, offset);
}

bool KML::Internal::Output::OutputKmlFile::save(kmlFs::path output)
{
	bool isKmz = boost::iequals(output.extension().string(), ".kmz");
	xercesc::DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation(_X("Core"));
	if (impl != nullptr)
	{
		xercesc::DOMDocument* doc = impl->createDocument(0, _X("kml"), 0);
		xercesc::DOMElement* kml = doc->getDocumentElement();

		if (ns.length() > 0)
			kml->setAttribute(_X("xmlns"), ns.c_str());

		if (document)
			document->save(doc, kml);

		xercesc::DOMImplementation* implementation = DOMImplementationRegistry::getDOMImplementation(_X("LS"));
		// Create a DOMLSSerializer which is used to serialize a DOM tree into an XML document
		xercesc::DOMLSSerializer* serializer = ((DOMImplementationLS*)implementation)->createLSSerializer();
		//pretty print the exported xml
		if (serializer->getDomConfig()->canSetParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true))
			serializer->getDomConfig()->setParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true);
		XMLFormatTarget* formatTarget;
		// Specify the target for the XML output
		if (isKmz)
		{
			//for KMZ build the output file in memory
			formatTarget = new MemBufFormatTarget();
		}
		else
		{
#ifdef XERCES_USE_U
			formatTarget = new LocalFileFormatTarget(output.u16string().c_str());
#else
			formatTarget = new LocalFileFormatTarget(output.c_str());
#endif
		}
		// Create a new empty output destination object
		xercesc::DOMLSOutput *domout = ((DOMImplementationLS*)implementation)->createLSOutput();
		// Set the stream to our target
		domout->setByteStream(formatTarget);
		// Write the serialized output to the destination
		serializer->write(doc, domout);

		//write the KMZ file
		if (isKmz)
		{
			auto target = static_cast<MemBufFormatTarget*>(formatTarget);
			createZipFile(output, "doc.kml", target->getRawBuffer(), target->getLen());
		}

		serializer->release();
		delete formatTarget;
		domout->release();

		doc->release();

		return true;
	}

	return false;
}

KML::Internal::Input::InputDocument::InputDocument(xercesc::DOMNode * elem)
	: schema(nullptr),
	  folder(nullptr)
{
	xercesc::DOMElement* el = dynamic_cast<DOMElement*>(elem);
	xerces_string localName = _X("Data");
	if (el != nullptr)
	{
		id = el->getAttribute(_X("id"));

		xercesc::DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			xerces_string name = node->getNodeName();

			if (!schema && iequals(name, _X("Schema")))
				schema = new InputSchema(node);
			else if (!folder && (iequals(name, _X("Folder")) || iequals(name, _X("Document"))))
				folder = new InputFolder(node);
			//the placemarks aren't in a folder they are direclty in the document
			else if (iequals(name, _X("Placemark")))
			{
				if (!folder)
					folder = new InputFolder(localName);
				folder->parsePlacemark(node);
			}
			else if (iequals(name, _X("name")))
				localName = node->getTextContent();
			else if (link.size() == 0 && iequals(name, _X("NetworkLink")))
			{
				xercesc::DOMNode* _link = findNode(node, _X("Link"));
				if (_link)
				{
					xercesc::DOMElement* href = dynamic_cast<xercesc::DOMElement*>(findNode(_link, _X("href")));
					if (href)
						this->link = href->getTextContent();
				}
			}

			node = node->getNextSibling();
		}
	}
}

KML::Internal::Input::InputDocument::~InputDocument()
{
	if (schema)
		delete schema;
	if (folder)
		delete folder;
}

void KML::Internal::Input::InputDocument::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("Document"));
	parent->appendChild(element);
	
	element->setAttribute(_X("id"), id.c_str());

	if (schema)
		schema->save(document, element);
	if (folder)
		folder->save(document, element);
}

KML::Internal::Input::InputFolder::InputFolder(xercesc::DOMNode * elem)
	: schema(nullptr)
{
	xercesc::DOMElement* el = dynamic_cast<xercesc::DOMElement*>(elem);
	if (el != nullptr)
	{
		xercesc::DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			if (name.length() == 0 && iequals(node->getNodeName(), _X("name")))
				name = node->getTextContent();
			else if (!schema && iequals(node->getNodeName(), _X("Schema")))
				schema = new InputSchema(node);
			else if (iequals(node->getNodeName(), _X("PlaceMark")))
				parsePlacemark(node);

			node = node->getNextSibling();
		}
	}
}

KML::Internal::Input::InputFolder::InputFolder(const xerces_string& name)
	: name(name),
	  schema(nullptr)
{
}

KML::Internal::Input::InputFolder::~InputFolder()
{
	if (schema)
		delete schema;
	for (auto it = placemark.begin(); it != placemark.end(); it++)
		delete *it;
	placemark.clear();
}

void KML::Internal::Input::InputFolder::parsePlacemark(xercesc::DOMNode* node)
{
	placemark.push_back(new InputPlacemark(node));
}

void KML::Internal::Input::InputFolder::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("Folder"));
	parent->appendChild(element);

	xercesc::DOMElement* nameElement = document->createElement(_X("name"));
	nameElement->setTextContent(name.c_str());
	element->appendChild(nameElement);
	
	if (schema)
		schema->save(document, element);
	for (auto it = placemark.begin(); it != placemark.end(); it++)
		(*it)->save(document, element);
}

KML::Internal::Input::InputSchema::InputSchema(xercesc::DOMNode * elem)
{
	xercesc::DOMElement* el = dynamic_cast<DOMElement*>(elem);
	if (el != nullptr)
	{
		id = el->getAttribute(_X("id"));
		name = el->getAttribute(_X("name"));

		DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			if (iequals(node->getNodeName(), _X("SimpleField")))
				simpleField.push_back(new SimpleField(node));

			node = node->getNextSibling();
		}
	}
}

KML::Internal::Input::InputSchema::~InputSchema()
{
	for (auto it = simpleField.begin(); it != simpleField.end(); it++)
		delete *it;
	simpleField.clear();
}

void KML::Internal::Input::InputSchema::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("Schema"));
	parent->appendChild(element);

	element->setAttribute(_X("name"), name.c_str());
	element->setAttribute(_X("id"), id.c_str());

	for (auto it = simpleField.begin(); it != simpleField.end(); it++)
		(*it)->save(document, element);
}

KML::Internal::Input::InputPlacemark::InputPlacemark(xercesc::DOMNode * elem)
	: style(nullptr),
	  extendedData(nullptr),
	  lineString(nullptr),
      _next(nullptr)
{
	xercesc::DOMElement* el = dynamic_cast<xercesc::DOMElement*>(elem);
	if (el != nullptr)
	{
		DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			if (iequals(node->getNodeName(), _X("name")))
				name = node->getTextContent();
			else if (iequals(node->getNodeName(), _X("Style")))
				style = new InputStyle(node);
			else if (iequals(node->getNodeName(), _X("ExtendedData")))
				extendedData = new InputExtendedData(node);
			else if (iequals(node->getNodeName(), _X("Polygon")))
				polygons.push_back(new Polygon(node));
			else if (iequals(node->getNodeName(), _X("MultiGeometry")))
			{
				//multigeometries can hold multiple polygons
				DOMNode* inode = node->getFirstChild();
				while (inode != nullptr)
				{
					if (iequals(inode->getNodeName(), _X("Polygon")))
						polygons.push_back(new Polygon(inode));
					inode = inode->getNextSibling();
				}
			}
			else if (iequals(node->getNodeName(), _X("LineString")))
				lineString = new LineString(node);
			else if (iequals(node->getNodeName(), _X("TimeStamp")))
			{
				auto when = findNode(node, _X("when"));
				if (when)
					time = when->getTextContent();
			}

			node = node->getNextSibling();
		}
	}
}

KML::Internal::Input::InputPlacemark::~InputPlacemark()
{
	if (style)
		delete style;
	if (extendedData)
		delete extendedData;
	for (auto p : polygons)
		delete p;
	polygons.clear();
	if (lineString)
		delete lineString;
}

void KML::Internal::Input::InputPlacemark::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("Placemark"));
	parent->appendChild(element);

	xercesc::DOMElement* nameElement = document->createElement(_X("name"));
	nameElement->setTextContent(name.c_str());
	element->appendChild(nameElement);

	if (style)
		style->save(document, element);
	if (extendedData)
		extendedData->save(document, element);
	if (polygons.size() == 1)
		polygons[0]->save(document, element);
	else if (polygons.size() > 1)
	{
		xercesc::DOMElement* multiElement = document->createElement(_X("MultiGeometry"));
		element->appendChild(multiElement);
		for (auto p : polygons)
			p->save(document, multiElement);
	}
	if (lineString)
		lineString->save(document, element);
}

KML::Internal::Input::InputExtendedData::InputExtendedData(xercesc::DOMNode * elem)
	: schemaData(nullptr)
{
	xercesc::DOMElement* el = dynamic_cast<DOMElement*>(elem);
	if (el != nullptr)
	{
		xercesc::DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			if (iequals(node->getNodeName(), _X("SchemaData")))
				schemaData = new InputSchemaData(node);

			node = node->getNextSibling();
		}
	}
}

KML::Internal::Input::InputExtendedData::~InputExtendedData()
{
	if (schemaData)
		delete schemaData;
}

void KML::Internal::Input::InputExtendedData::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("ExtendedData"));
	parent->appendChild(element);

	if (schemaData)
		schemaData->save(document, element);
}

KML::Internal::Input::InputSchemaData::InputSchemaData(xercesc::DOMNode * elem)
{
	xercesc::DOMElement* el = dynamic_cast<xercesc::DOMElement*>(elem);
	if (el != nullptr)
	{
		schemaUrl = el->getAttribute(_X("schemaUrl"));

		xercesc::DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			if (iequals(node->getNodeName(), _X("SimpleData")))
				simpleData.push_back(new SimpleData(node));

			node = node->getNextSibling();
		}
	}
}

KML::Internal::Input::InputSchemaData::~InputSchemaData()
{
	for (auto it = simpleData.begin(); it != simpleData.end(); it++)
		delete *it;
	simpleData.clear();
}

void KML::Internal::Input::InputSchemaData::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("SchemaData"));
	parent->appendChild(element);

	if (schemaUrl.length() > 0)
		element->setAttribute(_X("schemaUrl"), schemaUrl.c_str());

	for (auto it = simpleData.begin(); it != simpleData.end(); it++)
		(*it)->save(document, element);
}

KML::Internal::Input::InputStyle::InputStyle(xercesc::DOMNode * elem)
	: lineStyle(nullptr),
	  polyStyle(nullptr)
{
	xercesc::DOMElement* el = dynamic_cast<xercesc::DOMElement*>(elem);
	if (el != nullptr)
	{
		DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			if (!lineStyle && iequals(node->getNodeName(), _X("LineStyle")))
				lineStyle = new InputLineStyle(node);
			else if (!polyStyle && iequals(node->getNodeName(), _X("PolyStyle")))
				polyStyle = new PolyStyle(node);

			node = node->getNextSibling();
		}
	}
}

KML::Internal::Input::InputStyle::~InputStyle()
{
	if (lineStyle)
		delete lineStyle;
	if (polyStyle)
		delete polyStyle;
}

void KML::Internal::Input::InputStyle::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("Style"));
	parent->appendChild(element);

	if (lineStyle)
		lineStyle->save(document, element);
	if (polyStyle)
		polyStyle->save(document, element);
}

KML::Internal::Input::InputLineStyle::InputLineStyle(xercesc::DOMNode * elem)
{
	xercesc::DOMElement* el = dynamic_cast<DOMElement*>(elem);
	if (el != nullptr)
	{
		DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			if (iequals(node->getNodeName(), _X("color")))
			{
				color = node->getTextContent();
				break;
			}

			node = node->getNextSibling();
		}
	}
}

void KML::Internal::Input::InputLineStyle::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("LineStyle"));
	parent->appendChild(element);

	xercesc::DOMElement* colorElement = document->createElement(_X("color"));
	element->appendChild(colorElement);
	colorElement->setTextContent(color.c_str());
}

KML::Internal::SimpleField::SimpleField(xercesc::DOMNode * elem)
{
	xercesc::DOMElement* el = dynamic_cast<DOMElement*>(elem);
	if (el != nullptr)
	{
		name = el->getAttribute(_X("name"));
		type = el->getAttribute(_X("type"));
	}
}

KML::Internal::SimpleField::SimpleField(const SimpleField& other)
{
	name = other.name;
	type = other.type;
}

void KML::Internal::SimpleField::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("SimpleField"));
	parent->appendChild(element);

	element->setAttribute(_X("name"), name.c_str());
	element->setAttribute(_X("type"), type.c_str());
}

KML::Internal::SimpleData::SimpleData(xercesc::DOMNode * elem)
{
	xercesc::DOMElement* el = dynamic_cast<xercesc::DOMElement*>(elem);
	if (el != nullptr)
	{
		name = el->getAttribute(_X("name"));
		value = el->getTextContent();
	}
}

KML::Internal::SimpleData::SimpleData(const SimpleData& other)
{
	name = other.name;
	value = other.value;
}

void KML::Internal::SimpleData::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("SimpleData"));
	parent->appendChild(element);

	element->setAttribute(_X("name"), name.c_str());
	element->setTextContent(value.c_str());
}

KML::Internal::PolyStyle::PolyStyle(xercesc::DOMNode * elem)
{
	xercesc::DOMElement* el = dynamic_cast<xercesc::DOMElement*>(elem);
	if (el != nullptr)
	{
		DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			if (iequals(node->getNodeName(), _X("fill")))
			{
				fill = node->getTextContent();
				break;
			}

			node = node->getNextSibling();
		}
	}
}

KML::Internal::PolyStyle::PolyStyle()
{
	fill = _X("0");
}

KML::Internal::PolyStyle::PolyStyle(const PolyStyle& other)
{
	fill = other.fill;
}

void KML::Internal::PolyStyle::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("PolyStyle"));
	parent->appendChild(element);

	xercesc::DOMElement* fillElement = document->createElement(_X("fill"));
	element->appendChild(fillElement);
	fillElement->setTextContent(fill.c_str());
}

KML::Internal::Polygon::Polygon(xercesc::DOMNode * elem)
    : outerBoundaryIs(nullptr)
{
	xercesc::DOMElement* el = dynamic_cast<xercesc::DOMElement*>(elem);
	if (el != nullptr)
	{
		DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			if (!outerBoundaryIs && iequals(node->getNodeName(), _X("outerBoundaryIs")))
				outerBoundaryIs = new OuterBoundaryIs(node);

			node = node->getNextSibling();
		}
	}
}

KML::Internal::Polygon::Polygon(const Polygon& other)
{
	if (other.outerBoundaryIs)
		outerBoundaryIs = new OuterBoundaryIs(*other.outerBoundaryIs);
}

KML::Internal::Polygon::~Polygon()
{
	if (outerBoundaryIs)
		delete outerBoundaryIs;
}

void KML::Internal::Polygon::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("Polygon"));
	parent->appendChild(element);

	if (outerBoundaryIs)
		outerBoundaryIs->save(document, element);
}

KML::Internal::LineString::LineString() {
	coordinates = nullptr;
}

KML::Internal::LineString::LineString(xercesc::DOMNode * elem)
{
	coordinates = nullptr;
	xercesc::DOMElement* el = dynamic_cast<xercesc::DOMElement*>(elem);
	if (el != nullptr)
	{
		DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			if (iequals(node->getNodeName(), _X("coordinates"))) {
				coordinates = new Coordinates(node);
				break;
			}
			node = node->getNextSibling();
		}
	}
}

KML::Internal::LineString::LineString(const LineString& other)
{
	if (other.coordinates)
		coordinates = new Coordinates(*other.coordinates);
}

KML::Internal::LineString::~LineString()
{
	if (coordinates)
		delete coordinates;
}

void KML::Internal::LineString::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("LineString"));
	parent->appendChild(element);

	if (coordinates)
		coordinates->save(document, element);
}

KML::Internal::OuterBoundaryIs::OuterBoundaryIs(xercesc::DOMNode * elem)
    : linearRing(nullptr)
{
	xercesc::DOMElement* el = dynamic_cast<xercesc::DOMElement*>(elem);
	if (el != nullptr)
	{
		xercesc::DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			if (!linearRing && iequals(node->getNodeName(), _X("LinearRing")))
				linearRing = new LinearRing(node);

			node = node->getNextSibling();
		}
	}
}

KML::Internal::OuterBoundaryIs::OuterBoundaryIs(const OuterBoundaryIs& other)
{
	if (other.linearRing)
		linearRing = new LinearRing(*other.linearRing);
}

KML::Internal::OuterBoundaryIs::~OuterBoundaryIs()
{
	if (linearRing)
		delete linearRing;
}

void KML::Internal::OuterBoundaryIs::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	DOMElement* element = document->createElement(_X("outerBoundaryIs"));
	parent->appendChild(element);

	if (linearRing)
		linearRing->save(document, element);
}

KML::Internal::LinearRing::LinearRing(xercesc::DOMNode * elem)
    : coordinates(nullptr)
{
	xercesc::DOMElement* el = dynamic_cast<DOMElement*>(elem);
	if (el != nullptr)
	{
		xercesc::DOMNode* node = el->getFirstChild();
		while (node != nullptr)
		{
			if (!coordinates && iequals(node->getNodeName(), _X("coordinates")))
				coordinates = new Coordinates(node);

			node = node->getNextSibling();
		}
	}
}

KML::Internal::LinearRing::LinearRing(const LinearRing& other)
    : coordinates(nullptr)
{
	if (other.coordinates)
		coordinates = new Coordinates(*other.coordinates);
}

KML::Internal::LinearRing::~LinearRing()
{
	if (coordinates)
		delete coordinates;
}

void KML::Internal::LinearRing::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("LinearRing"));
	parent->appendChild(element);

	if (coordinates)
		coordinates->save(document, element);
}

KML::Internal::Coordinates::Coordinates(xercesc::DOMNode * elem)
{
	value = elem->getTextContent();
}

KML::Internal::Coordinates::Coordinates(const Coordinates& other)
{
	value = other.value;
}

void KML::Internal::Coordinates::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("coordinates"));
	parent->appendChild(element);
	element->setTextContent(value.c_str());
}

KML::Internal::Output::OutputDocument::OutputDocument(Input::InputDocument * document, const HSS_Time::WTimeSpan& offset)
	: folder(nullptr),
	  schema(nullptr)
{
	id = document->id;
	if (document->folder)
		folder = new OutputFolder(document->folder, offset);
	if (document->schema)
		schema = new OutputSchema(document->schema);
}

KML::Internal::Output::OutputDocument::~OutputDocument()
{
	if (folder)
		delete folder;
	if (schema)
		delete schema;
}

void KML::Internal::Output::OutputDocument::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("Document"));
	parent->appendChild(element);

	if (folder)
		folder->save(document, element);
	if (schema)
		schema->save(document, element);
}

KML::Internal::Output::OutputFolder::OutputFolder(Input::InputFolder * folder, const HSS_Time::WTimeSpan& offset)
	: schema(nullptr)
{
	name = folder->name;
	if (folder->schema)
		schema = new OutputSchema(folder->schema);
	for (auto it = folder->placemark.begin(); it != folder->placemark.end(); it++)
		placemark.push_back(new OutputPlacemark((*it), offset));
}

KML::Internal::Output::OutputFolder::~OutputFolder()
{
	if (schema)
		delete schema;
	for (auto it = placemark.begin(); it != placemark.end(); it++)
		delete (*it);
	placemark.clear();
}

void KML::Internal::Output::OutputFolder::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("Folder"));
	parent->appendChild(element);

	if (schema)
		schema->save(document, element);
	for (auto it = placemark.begin(); it != placemark.end(); it++)
		(*it)->save(document, element);

	xercesc::DOMElement* nameElement = document->createElement(_X("name"));
	nameElement->setTextContent(name.c_str());
	element->appendChild(nameElement);
}

KML::Internal::Output::OutputSchema::OutputSchema(Input::InputSchema* schema)
{
	id = schema->id;
	name = schema->name;
	for (auto it = schema->simpleField.begin(); it != schema->simpleField.end(); it++)
		simpleField.push_back(new SimpleField(*(*it)));
}

KML::Internal::Output::OutputSchema::~OutputSchema()
{
	for (auto it = simpleField.begin(); it != simpleField.end(); it++)
		delete (*it);
	simpleField.clear();
}

void KML::Internal::Output::OutputSchema::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("Schema"));
	parent->appendChild(element);

	element->setAttribute(_X("name"), name.c_str());
	element->setAttribute(_X("id"), id.c_str());

	for (auto it = simpleField.begin(); it != simpleField.end(); it++)
		(*it)->save(document, element);
}

KML::Internal::Output::OutputPlacemark::OutputPlacemark(Input::InputPlacemark* placemark, const HSS_Time::WTimeSpan& offset)
	: style(nullptr),
	  extendedData(nullptr),
	  lineString(nullptr),
	  timeSpan(nullptr)
{
	xerces_string start;
	xerces_string end;

	WorldLocation location;
	location.m_timezone(offset);
	WTimeManager manager(location);
	WTime endTime(&manager);

#ifndef XERCES_USE_U
	std::wstring_convert<std::codecvt_utf8_utf16<xerces_char>> converter;
#endif

	WTime startTime(&manager);
	if (placemark->time.length() > 0)
	{
		start = placemark->time;
#ifdef XERCES_USE_U
		std::string cstart(utf16_to_utf8(start));
		startTime.ParseDateTime(cstart, WTIME_FORMAT_STRING_ISO8601);
		start = utf8_to_utf16(startTime.ToString(WTIME_FORMAT_STRING_ISO8601));
#else
		startTime.ParseDateTime(start, WTIME_FORMAT_STRING_ISO8601);
		start = converter.from_bytes(startTime.ToString(WTIME_FORMAT_STRING_ISO8601));
#endif
	}
	else
	{
		for (auto it = placemark->extendedData->schemaData->simpleData.begin(); it != placemark->extendedData->schemaData->simpleData.end(); it++)
		{
			if (iequals((*it)->name, _X("TIMESTAMP")))
			{
				start = (*it)->value;
#ifdef XERCES_USE_U
				std::string cstart(utf16_to_utf8(start));
				startTime.ParseDateTime(cstart, WTIME_FORMAT_DATE | WTIME_FORMAT_TIME | WTIME_FORMAT_STRING_YYYY_MM_DD | WTIME_FORMAT_AS_LOCAL);
				start = utf8_to_utf16(startTime.ToString(WTIME_FORMAT_STRING_ISO8601));
#else
				startTime.ParseDateTime(start, WTIME_FORMAT_DATE | WTIME_FORMAT_TIME | WTIME_FORMAT_STRING_YYYY_MM_DD | WTIME_FORMAT_AS_LOCAL);
				start = converter.from_bytes(startTime.ToString(WTIME_FORMAT_STRING_ISO8601));
#endif

				break;
			}
		}
	}
	if (start.length() > 0)
	{
		InputPlacemark* next = placemark->_next;
		while (next && end.length() == 0)
		{
			if (next->time.length() > 0)
			{
				end = next->time;
#ifdef XERCES_USE_U
				std::string cend(utf16_to_utf8(end));
				endTime.ParseDateTime(cend, WTIME_FORMAT_STRING_ISO8601);
#else
				endTime.ParseDateTime(end, WTIME_FORMAT_STRING_ISO8601);
#endif
			}
			else
			{
				for (auto it = next->extendedData->schemaData->simpleData.begin(); it != next->extendedData->schemaData->simpleData.end(); it++)
				{
					if (iequals((*it)->name, _X("TIMESTAMP")))
					{
						end = (*it)->value;
#ifdef XERCES_USE_U
						std::string cend(utf16_to_utf8(end));
						endTime.ParseDateTime(cend, WTIME_FORMAT_DATE | WTIME_FORMAT_TIME | WTIME_FORMAT_STRING_YYYY_MM_DD | WTIME_FORMAT_AS_LOCAL);
#else
						endTime.ParseDateTime(end, WTIME_FORMAT_DATE | WTIME_FORMAT_TIME | WTIME_FORMAT_STRING_YYYY_MM_DD | WTIME_FORMAT_AS_LOCAL);
#endif
						break;
					}
				}
			}

			if (end.length())
			{
				//only break if a time in the future has been found, will work around having multiple placemarks with the same time
				if (endTime.GetTime(0) > startTime.GetTime(0))
					break;
				else
					end.clear();
			}

			next = next->_next;
		}

		if (end.length())
		{
			endTime -= WTimeSpan(1);
			if (endTime.GetTime(0) > startTime.GetTime(0))
			{
#ifdef XERCES_USE_U
				end = utf8_to_utf16(endTime.ToString(WTIME_FORMAT_STRING_ISO8601));
#else
				end = converter.from_bytes(endTime.ToString(WTIME_FORMAT_STRING_ISO8601));
#endif
			}
			else
				end = _X("");
		}
	}
	timeSpan = new OutputTimeSpan(start, end);
	name = placemark->name;
	if (placemark->style)
		style = new OutputStyle(placemark->style);
	else
		style = new OutputStyle();
	int32_t width = 1;
	xerces_string color = _X("ff0000ff");
	if (placemark->extendedData) {
		for (auto it = placemark->extendedData->schemaData->simpleData.begin(); it != placemark->extendedData->schemaData->simpleData.end(); it++)
		{
			if (iequals((*it)->name, _X("WIDTH")))
				width = (int)(stod((*it)->value.c_str()));
			else if (iequals((*it)->name, _X("COLOR")))
				color = (*it)->value;
		}
	}
	style->lineStyle->width = width;
	style->lineStyle->color = color;
	if (placemark->extendedData)
		extendedData = new OutputExtendedData(placemark->extendedData);
	for (auto p : placemark->polygons)
		polygons.push_back(new Polygon(*p));
	if (placemark->lineString)
		lineString = new LineString(*placemark->lineString);
}

KML::Internal::Output::OutputPlacemark::~OutputPlacemark()
{
	if (style)
		delete style;
	if (extendedData)
		delete extendedData;
	for (auto p : polygons)
		delete p;
	polygons.clear();
	if (lineString)
		delete lineString;
	if (timeSpan)
		delete timeSpan;
}

void KML::Internal::Output::OutputPlacemark::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("Placemark"));
	parent->appendChild(element);

	if (timeSpan)
		timeSpan->save(document, element);

	xercesc::DOMElement* nameElement = document->createElement(_X("name"));
	nameElement->setTextContent(name.c_str());
	element->appendChild(nameElement);

	if (style)
		style->save(document, element);
	if (extendedData)
		extendedData->save(document, element);
	if (polygons.size() == 1)
		polygons[0]->save(document, element);
	else if (polygons.size() > 1)
	{
		xercesc::DOMElement* multiElement = document->createElement(_X("MultiGeometry"));
		element->appendChild(multiElement);
		for (auto p : polygons)
			p->save(document, multiElement);
	}
	if (lineString)
		lineString->save(document, element);
}

KML::Internal::Output::OutputTimeSpan::OutputTimeSpan(xerces_string start, xerces_string end)
	: begin(start), end(end)
{
}

void KML::Internal::Output::OutputTimeSpan::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	if (begin.length() || end.length())
	{
		xercesc::DOMElement* element = document->createElement(_X("TimeSpan"));
		parent->appendChild(element);

		if (begin.length())
		{
			xercesc::DOMElement* beginElement = document->createElement(_X("begin"));
			beginElement->setTextContent(begin.c_str());
			element->appendChild(beginElement);
		}

		if (end.length())
		{
			xercesc::DOMElement* endElement = document->createElement(_X("end"));
			endElement->setTextContent(end.c_str());
			element->appendChild(endElement);
		}
	}
}

KML::Internal::Output::OutputExtendedData::OutputExtendedData(Input::InputExtendedData* data)
	: schemaData(nullptr)
{
	if (data->schemaData)
		schemaData = new OutputSchemaData(data->schemaData);
}

KML::Internal::Output::OutputExtendedData::~OutputExtendedData()
{
	if (schemaData)
		delete schemaData;
}

void KML::Internal::Output::OutputExtendedData::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("ExtendedData"));
	parent->appendChild(element);

	if (schemaData)
		schemaData->save(document, element);
}

KML::Internal::Output::OutputSchemaData::OutputSchemaData(Input::InputSchemaData* data)
{
	schemaUrl = data->schemaUrl;
	for (auto it = data->simpleData.begin(); it != data->simpleData.end(); it++)
	{
		if (!iequals((*it)->name, _X("WIDTH")) && !iequals((*it)->name, _X("COLOR")) && !iequals((*it)->name, _X("TIMESTAMP")))
			simpleData.push_back(new SimpleData(*(*it)));
	}
}

KML::Internal::Output::OutputSchemaData::~OutputSchemaData()
{
	for (auto it = simpleData.begin(); it != simpleData.end(); it++)
		delete (*it);
	simpleData.clear();
}

void KML::Internal::Output::OutputSchemaData::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("SchemaData"));
	parent->appendChild(element);

	for (auto it = simpleData.begin(); it != simpleData.end(); it++)
		(*it)->save(document, element);
}

KML::Internal::Output::OutputStyle::OutputStyle(Input::InputStyle* style)
	: lineStyle(nullptr),
	  polyStyle(nullptr)
{
	if (style->lineStyle)
		lineStyle = new OutputLineStyle(style->lineStyle);
	if (style->polyStyle)
		polyStyle = new PolyStyle(*style->polyStyle);
}

KML::Internal::Output::OutputStyle::OutputStyle()
	: lineStyle(nullptr),
	polyStyle(nullptr)
{
	lineStyle = new OutputLineStyle();
	polyStyle = new PolyStyle();
}

KML::Internal::Output::OutputStyle::~OutputStyle()
{
	if (lineStyle)
		delete lineStyle;
	if (polyStyle)
		delete polyStyle;
}

void KML::Internal::Output::OutputStyle::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("Style"));
	parent->appendChild(element);

	if (lineStyle)
		lineStyle->save(document, element);
	if (polyStyle)
		polyStyle->save(document, element);
}

KML::Internal::Output::OutputLineStyle::OutputLineStyle(Input::InputLineStyle* style)
{
	color = style->color;
	width = 1;
}

KML::Internal::Output::OutputLineStyle::OutputLineStyle()
{
	color = _X("ff0000ff");
	width = 1;
}

void KML::Internal::Output::OutputLineStyle::save(xercesc::DOMDocument* document, xercesc::DOMElement* parent)
{
	xercesc::DOMElement* element = document->createElement(_X("LineStyle"));
	parent->appendChild(element);

	xercesc::DOMElement* colorElement = document->createElement(_X("color"));
	colorElement->setTextContent(color.c_str());
	element->appendChild(colorElement);

	xercesc::DOMElement* widthElement = document->createElement(_X("width"));

#ifdef XERCES_USE_U
	std::string ctemp = std::to_string(width);
	xerces_string temp = utf8_to_utf16(ctemp);
#else
	xerces_string temp = std::to_wstring(width);
#endif
	widthElement->setTextContent(temp.c_str());
	element->appendChild(widthElement);
}
