/**
 * WISE_Processing_Lib: kmlinternal.h
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

#pragma once

#include "types.h"
#include <vector>
#include "filesystem.hpp"
#include "WTime.h"

#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMDocumentType.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMImplementation.hpp>
#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMNodeIterator.hpp>
#include <xercesc/dom/DOMNodeList.hpp>
#include <xercesc/dom/DOMText.hpp>

#include <xercesc/framework/MemBufInputSource.hpp>
#include <xercesc/framework/MemBufFormatTarget.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/XMLUni.hpp>

#include <xercesc/framework/LocalFileFormatTarget.hpp>

#define XERCES_USE_U
#ifdef XERCES_USE_U
typedef std::u16string xerces_string;
typedef char16_t xerces_char;
#define _X(text) u##text
#else
typedef std::wstring xerces_string;
typedef wchar_t xerces_char;
#define _X(text) L##text
#endif

#if __cplusplus<201703L || (GCC_VERSION > NO_GCC && GCC_VERSION < GCC_8)
namespace kmlFs = std::experimental::filesystem;
#else
namespace kmlFs = std::filesystem;
#endif



extern void initializeXML();
extern void deinitializeXML();

namespace KML::Internal
{
	static std::string utf16_to_utf8(const xerces_string &utf16_string);
	static xerces_string utf8_to_utf16(const std::string &utf16_string);

	static int stoi(const xerces_string& _Str, size_t *_Idx = 0, int _Base = 10);
	static double stod(const xerces_string& _Str, size_t *_Idx = 0);

	class Coordinates
	{
	public:
		explicit Coordinates(xercesc::DOMNode* elem);
		Coordinates(const Coordinates& other);
		void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

		xerces_string value;
	};

	class LinearRing
	{
	public:
		explicit LinearRing(xercesc::DOMNode* elem);
		LinearRing(const LinearRing& other);
		virtual ~LinearRing();
		void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

		Coordinates* coordinates;
	};

	class OuterBoundaryIs
	{
	public:
		explicit OuterBoundaryIs(xercesc::DOMNode* elem);
		OuterBoundaryIs(const OuterBoundaryIs& other);
		virtual ~OuterBoundaryIs();
		void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

		LinearRing* linearRing;
	};

	class LineString
	{
	public:
		explicit LineString(xercesc::DOMNode* elem);
		LineString();
		LineString(const LineString& other);
		virtual ~LineString();
		void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

		Coordinates* coordinates;
	};

	class Polygon
	{
	public:
		explicit Polygon(xercesc::DOMNode* elem);
		Polygon(const Polygon& other);
		virtual ~Polygon();
		void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

		OuterBoundaryIs* outerBoundaryIs;
	};

	class PolyStyle
	{
	public:
		explicit PolyStyle(xercesc::DOMNode* elem);
		PolyStyle();
		PolyStyle(const PolyStyle& other);
		void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

		xerces_string fill;
	};

	class SimpleData
	{
	public:
		explicit SimpleData(xercesc::DOMNode* elem);
		SimpleData(const SimpleData& other);
		void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

		xerces_string name;
		xerces_string value;
	};

	class SimpleField
	{
	public:
		explicit SimpleField(xercesc::DOMNode* elem);
		SimpleField(const SimpleField& other);
		void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

		xerces_string name;
		xerces_string type;
	};

	namespace Input
	{
		class InputLineStyle
		{
		public:
			explicit InputLineStyle(xercesc::DOMNode* elem);
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			xerces_string color;
		};

		class InputStyle
		{
		public:
			explicit InputStyle(xercesc::DOMNode* elem);
			virtual ~InputStyle();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			InputLineStyle* lineStyle;
			PolyStyle* polyStyle;
		};

		class InputSchemaData
		{
		public:
			explicit InputSchemaData(xercesc::DOMNode* elem);
			virtual ~InputSchemaData();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			xerces_string schemaUrl;
			std::vector<SimpleData*> simpleData;
		};

		class InputExtendedData
		{
		public:
			explicit InputExtendedData(xercesc::DOMNode* elem);
			virtual ~InputExtendedData();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			InputSchemaData* schemaData;
		};

		class InputPlacemark
		{
		public:
			explicit InputPlacemark(xercesc::DOMNode* elem);
			virtual ~InputPlacemark();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			xerces_string name;
			InputStyle* style;
			InputExtendedData* extendedData;
			std::vector<Polygon*> polygons;
			LineString* lineString;
			InputPlacemark* _next;
			xerces_string time;
		};

		class InputSchema
		{
		public:
			explicit InputSchema(xercesc::DOMNode* elem);
			virtual ~InputSchema();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			xerces_string id;
			xerces_string name;
			std::vector<SimpleField*> simpleField;
		};

		class InputFolder
		{
		public:
			explicit InputFolder(xercesc::DOMNode* elem);
			explicit InputFolder(const xerces_string& name);
			virtual ~InputFolder();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			void parsePlacemark(xercesc::DOMNode* node);

			xerces_string name;
			InputSchema* schema;
			std::vector<InputPlacemark*> placemark;
		};

		class InputDocument
		{
		public:
			explicit InputDocument(xercesc::DOMNode* elem);
			virtual ~InputDocument();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			xerces_string id;
			InputFolder* folder;
			InputSchema* schema;
			xerces_string link;
		};

		class InputKmlFile
		{
		public:
			explicit InputKmlFile(kmlFs::path input);
			virtual ~InputKmlFile();
			bool save(kmlFs::path output);

			xerces_string ns;
			InputDocument* document;

		protected:
			bool initialize(const kmlFs::path& input, const std::string& kmzPath);
		};
	}

	namespace Output
	{
		class OutputLineStyle
		{
		public:
			explicit OutputLineStyle(Input::InputLineStyle* style);
			OutputLineStyle();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			xerces_string color;
			std::int32_t width;
		};

		class OutputStyle
		{
		public:
			explicit OutputStyle(Input::InputStyle* style);
			OutputStyle();
			virtual ~OutputStyle();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			OutputLineStyle* lineStyle;
			PolyStyle* polyStyle;
		};

		class OutputSchemaData
		{
		public:
			explicit OutputSchemaData(Input::InputSchemaData* data);
			virtual ~OutputSchemaData();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			xerces_string schemaUrl;
			std::vector<SimpleData*> simpleData;
		};

		class OutputExtendedData
		{
		public:
			explicit OutputExtendedData(Input::InputExtendedData* data);
			virtual ~OutputExtendedData();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			OutputSchemaData* schemaData;
		};

		class OutputTimeSpan
		{
		public:
			explicit OutputTimeSpan(xerces_string start, xerces_string end);
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			xerces_string begin;
			xerces_string end;
		};

		class OutputPlacemark
		{
		public:
			explicit OutputPlacemark(Input::InputPlacemark* placemark, const HSS_Time::WTimeSpan& offset);
			virtual ~OutputPlacemark();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			xerces_string name;
			OutputStyle* style;
			OutputExtendedData* extendedData;
			std::vector<Polygon*> polygons;
			LineString* lineString;
			OutputTimeSpan* timeSpan;
		};

		class OutputSchema
		{
		public:
			explicit OutputSchema(Input::InputSchema* schema);
			virtual ~OutputSchema();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			xerces_string id;
			xerces_string name;
			std::vector<SimpleField*> simpleField;
		};

		class OutputFolder
		{
		public:
			explicit OutputFolder(Input::InputFolder* folder, const HSS_Time::WTimeSpan& offset);
			virtual ~OutputFolder();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			xerces_string name;
			OutputSchema* schema;
			std::vector<OutputPlacemark*> placemark;
		};

		class OutputDocument
		{
		public:
			explicit OutputDocument(Input::InputDocument* document, const HSS_Time::WTimeSpan& offset);
			virtual ~OutputDocument();
			void save(xercesc::DOMDocument* document, xercesc::DOMElement* parent);

			xerces_string id;
			OutputFolder* folder;
			OutputSchema* schema;
		};

		class OutputKmlFile
		{
		public:
			explicit OutputKmlFile(const KML::Internal::Input::InputKmlFile* input, const HSS_Time::WTimeSpan& offset);
			virtual bool save(kmlFs::path output);

			xerces_string ns;
			OutputDocument* document;
		};
	}
}

namespace Java::Internal
{
	void read_job_directory(const kmlFs::path& path, std::string& job_directory);
}
