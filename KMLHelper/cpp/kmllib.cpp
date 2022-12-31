/**
 * WISE_Processing_Lib: kmllib.cpp
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

#include "kmllib.h"
#include "kmlinternal.h"

using namespace xercesc;


void Java::read_job_directory(const kmlFs::path& path, std::string& job_directory)
{
	initializeXML();
	Java::Internal::read_job_directory(path, job_directory);
	deinitializeXML();
}

KML::KmlHelper::KmlHelper(const kmlFs::path& input) :
	m_errors(0)
{
	initializeXML();
	m_inputFile = new KML::Internal::Input::InputKmlFile(input);
}

KML::KmlHelper::~KmlHelper()
{
	if (m_inputFile)
		delete m_inputFile;

	deinitializeXML();
}

bool KML::KmlHelper::process(const kmlFs::path& output)
{
	return process(output, HSS_Time::WTimeSpan(0));
}

bool KML::KmlHelper::process(const kmlFs::path& output, const HSS_Time::WTimeSpan& offset)
{
	if (m_inputFile)
	{
		KML::Internal::Output::OutputKmlFile outkml(m_inputFile, offset);
		return outkml.save(output);
	}
	return false;
}
