/**
 * WISE_Processing_Lib: kmllib.h
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
#include "filesystem.hpp"
#include "WTime.h"
#include "kmllib_cfg.h"

namespace kmlFs = fs;


namespace KML
{
	namespace Internal::Input
	{
		class InputKmlFile;
	}
	class KML_LIB_API KmlHelper
	{
	public:
		/// <summary>
		/// Initialize the helper class with an input KML file. <paramref name="input"/> must
		/// reference an existing file that conforms to the expected KML format.
		/// </summary>
		/// <param name="input">The location of the KML file to parse.</param>
		explicit KmlHelper(const kmlFs::path& input);
		virtual ~KmlHelper();

		/// <summary>
		/// Process the input KML file and write the results to a file.
		/// </summary>
		/// <param name="output">The location to write the processed KML file to. Will be overwritten if it exists.</param>
		bool process(const kmlFs::path& output);

		/// <summary>
		/// Process the input KML file and write the results to a file.
		/// </summary>
		/// <param name="output">The location to write the processed KML file to. Will be overwritten if it exists.</param>
		/// <param name="timezone">The timezone offset to write to the output file.</param>
		bool process(const kmlFs::path& output, const HSS_Time::WTimeSpan& offset);

		/// <summary>
		/// Get an indicator of any errors that occurred while processing the KML file.
		/// </summary>
		inline std::int16_t GetErrors() { return m_errors; }
		/// <summary>
		/// Is the KML helper valid. If it is not valid <see cref="KmlHelper.GetErrors()"/> will contain details of the error.
		/// </summary>
		inline bool IsValid() { return m_errors == 0; }

	private:
		std::int16_t m_errors;
		KML::Internal::Input::InputKmlFile* m_inputFile;
	};
}

namespace Java
{
	KML_LIB_API void read_job_directory(const kmlFs::path& path, std::string& job_directory);
}
