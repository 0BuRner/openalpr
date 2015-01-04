/*
 * Copyright (c) 2015 Dr. Masroor Ehsan
 *
 * This file is part of OpenAlpr.Net.
 *
 * OpenAlpr.Net is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License
 * version 3 as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "openalpr-net.h"
#include "alpr.h"
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#using <mscorlib.dll>
//#include <msclr\marshal.h>
#include <msclr\marshal_cppstd.h>

using namespace System;
using namespace msclr::interop;
using namespace System::Collections::Generic;
using namespace System::Runtime::InteropServices;

namespace openalprnet {

	ref class StringConverter
	{
	public:
		static std::vector<unsigned char> ToVector(array<unsigned char>^ src)
		{
			std::vector<unsigned char> result(src->Length);
			pin_ptr<unsigned char> pin(&src[0]);
			unsigned char *first(pin), *last(pin + src->Length);
			std::copy(first, last, result.begin());
			return result;
		}

		static System::String^ ToManagedString(std::string s)
		{
			return gcnew String(s.c_str());
		}

		static std::string ToStlString(System::String^ s)
		{
			IntPtr ptr = Marshal::StringToHGlobalAnsi(s);
			if(ptr != IntPtr::Zero)
			{
				std::string tmp(reinterpret_cast<char*>(static_cast<void*>(ptr)));
				Marshal::FreeHGlobal(ptr);
				return tmp;
			}
			return std::string();
		}
	};

	public ref class AlprPlateNet
	{
	public:
		AlprPlateNet(AlprPlate plate){
			//_characters = marshal_as<System::String^>(plate.characters);
			m_characters = StringConverter::ToManagedString(plate.characters);
			m_overall_confidence=plate.overall_confidence;
			m_matches_template=plate.matches_template;
		}

		property System::String^ characters {
			System::String^ get() {
				return m_characters;
			}
		}

		property float overall_confidence {
			float get() {
				return m_overall_confidence;
			}
		}

		property bool matches_template {
			bool get() {
				return m_matches_template;
			}
		}

	private:
		System::String^ m_characters;
		float m_overall_confidence;
		bool m_matches_template;
	};

	public ref class AlprResultNet
	{
	public:
		AlprResultNet() : m_Impl( new AlprResult ) {}

		AlprResultNet(AlprResult* result) : m_Impl( result ) {}

		property int requested_topn {
			int get() {
				return m_Impl->requested_topn;
			}
		}
		
		property int regionConfidence {
			int get() {
				return m_Impl->regionConfidence;
			}
		}
		
		property System::String^ region {
			System::String^ get() {
				return StringConverter::ToManagedString(m_Impl->region);
			}
		}

		property int result_count {
			int get() {
				return m_Impl->result_count;
			}
		}

		property AlprPlateNet^ bestPlate {
			AlprPlateNet^ get() {
				AlprPlateNet^ result = gcnew AlprPlateNet(m_Impl->bestPlate);
				return result;
			}
		}

		property List<AlprPlateNet^>^ topNPlates {
			List<AlprPlateNet^>^ get() {
				List<AlprPlateNet^>^ list = gcnew List<AlprPlateNet^>(m_Impl->topNPlates.size());
				for (std::vector<AlprPlate>::iterator itr = m_Impl->topNPlates.begin(); itr != m_Impl->topNPlates.end(); itr++)
				{
					list->Add(gcnew AlprPlateNet(*itr));
				}
				return list;
			}
		}

		property float processing_time_ms {
			float get() {
				return m_Impl->processing_time_ms;
			}
		}

		/*
		AlprCoordinate plate_points[4];
		*/
	private:
		AlprResult * m_Impl;
	};

	public ref class AlprNet {
	public:
		// Allocate the native object on the C++ Heap via a constructor
		AlprNet(System::String^ country, System::String^ configFile) : m_Impl( new Alpr(marshal_as<std::string>(country), marshal_as<std::string>(configFile)) ) { }

		// Deallocate the native object on a destructor
		~AlprNet(){
			delete m_Impl;
		}

		property int TopN {
			int get() {
				return m_topN;
			}
			void set( int topn ){
				m_topN = topn;
				m_Impl->setTopN(topn);
			}
		}

		property bool DetectRegion {
			bool get() {
				return m_detectRegion;
			}
			void set( bool detectRegion ) {
				m_detectRegion = detectRegion;
				m_Impl->setDetectRegion(detectRegion);
			}
		}

		property System::String^ DefaultRegion {
			System::String^ get() {
				return m_defaultRegion;
			}
			void set( System::String^ region ){
				m_defaultRegion = region;
				m_Impl->setDefaultRegion(marshal_as<std::string>(region));
			}
		}

		//std::vector<AlprResult> recognize(std::string filepath);
		//std::vector<AlprResult> recognize(std::string filepath, std::vector<AlprRegionOfInterest> regionsOfInterest);
		//std::vector<AlprResult> recognize(std::vector<unsigned char> imageBuffer);
		//std::vector<AlprResult> recognize(std::vector<unsigned char> imageBuffer, std::vector<AlprRegionOfInterest> regionsOfInterest);
		List<AlprResultNet^>^ recognize(System::String^ filepath) {
			m_results = new std::vector<AlprResult>(m_Impl->recognize(marshal_as<std::string>(filepath)));
			std::vector<AlprResult>& runList = *m_results;
			std::vector<AlprResult>::iterator itr;
			List<AlprResultNet^>^ list = gcnew List<AlprResultNet^>(runList.size());
			for (itr = runList.begin(); itr != runList.end(); itr++)
			{
				list->Add(gcnew AlprResultNet(&*itr));
			}
			return list;
		}

		List<AlprResultNet^>^ recognize(cli::array<unsigned char>^ imageBuffer) {
			std::vector<unsigned char> p = StringConverter::ToVector(imageBuffer);
			m_results = new std::vector<AlprResult>(m_Impl->recognize(p));
			std::vector<AlprResult>& runList = *m_results;
			std::vector<AlprResult>::iterator itr;
			List<AlprResultNet^>^ list = gcnew List<AlprResultNet^>(runList.size());
			for (itr = runList.begin(); itr != runList.end(); itr++)
			{
				list->Add(gcnew AlprResultNet(&*itr));
			}
			return list;
		}

		bool isLoaded() {
			return m_Impl->isLoaded();
		}

		static System::String^ getVersion() {
			return StringConverter::ToManagedString(Alpr::getVersion());
		}

		System::String^ toJson() {
			std::string json = m_Impl->toJson(*m_results, -1);
			return StringConverter::ToManagedString(json);
		}

	protected:
		// Deallocate the native object on the finalizer just in case no destructor is called
		!AlprNet() {
			delete m_Impl;
		}

	private:
		Alpr * m_Impl;
		std::vector<AlprResult>* m_results;
		int m_topN;
		bool m_detectRegion;
		System::String^ m_defaultRegion;
	};
}