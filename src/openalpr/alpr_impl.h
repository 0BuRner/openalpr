/*
 * Copyright (c) 2013 New Designs Unlimited, LLC
 * Opensource Automated License Plate Recognition [http://www.openalpr.com]
 * 
 * This file is part of OpenAlpr.
 * 
 * OpenAlpr is free software: you can redistribute it and/or modify
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


#ifndef ALPRIMPL_H
#define ALPRIMPL_H

#include <list>

#include "alpr.h"
#include "config.h"

#include "regiondetector.h"
#include "licenseplatecandidate.h"
#include "stateidentifier.h"
#include "charactersegmenter.h"
#include "ocr.h"

#include "cjson.h"

#include <opencv2/core/core.hpp>
#include "opencv2/ocl/ocl.hpp"

#include "tinythread/tinythread.h"

#define DEFAULT_TOPN 25
#define DEFAULT_DETECT_REGION false

class AlprImpl
{

  public:
    AlprImpl(const std::string country, const std::string runtimeDir = "");
    virtual ~AlprImpl();

    std::vector<AlprResult> recognize(cv::Mat img);
    
    void applyRegionTemplate(AlprResult* result, std::string region);
    
    void setDetectRegion(bool detectRegion);
    void setTopN(int topn);
    void setDefaultRegion(string region);
    
    std::string toJson(const vector<AlprResult> results);
    
    Config* config;
    
  private:
    
    int getNumThreads();
    cJSON* createJsonObj(const AlprResult* result);
    
    RegionDetector* plateDetector;
    vector<StateIdentifier*> stateIdentifiers;
    vector<OCR*> ocrs;
  
    int topN;
    bool detectRegion;
    std::string defaultRegion;
    
    
    
};

class PlateDispatcher
{
  public:
    PlateDispatcher(vector<Rect> plateRegions, cv::Mat* image, 
		    Config* config,
		    vector<StateIdentifier*> stateIdentifiers,
		    vector<OCR*> ocrs,
		    int topN, bool detectRegion, std::string defaultRegion) 
    {
      this->plateRegions = plateRegions;
      this->frame = image;
      
      this->config = config;
      this->stateIdentifiers = stateIdentifiers;
      this->ocrs = ocrs;
      this->topN = topN;
      this->detectRegion = detectRegion;
      this->defaultRegion = defaultRegion;
      
      this->threadCounter = 0;
    }

    cv::Mat getImageCopy()
    {
      tthread::lock_guard<tthread::mutex> guard(mMutex);

      Mat img(this->frame->size(), this->frame->type());
      this->frame->copyTo(img);
      
      return img;
    }
    
    int getUniqueThreadId()
    {
      tthread::lock_guard<tthread::mutex> guard(mMutex);
      
      return threadCounter++;
    }

    bool hasPlate()
    {
      bool plateAvailable;
      mMutex.lock();
      plateAvailable = plateRegions.size() > 0;
      mMutex.unlock();
      return plateAvailable;
    }
    Rect nextPlate()
    {
      tthread::lock_guard<tthread::mutex> guard(mMutex);
      
      Rect plateRegion = plateRegions[plateRegions.size() - 1];
      plateRegions.pop_back();
      
      return plateRegion;
    }
    
    void addResult(AlprResult recognitionResult)
    {
      tthread::lock_guard<tthread::mutex> guard(mMutex);
      recognitionResults.push_back(recognitionResult);
    }
    
    vector<AlprResult> getRecognitionResults()
    {
      return recognitionResults;
    }
    

    vector<StateIdentifier*> stateIdentifiers;
    vector<OCR*> ocrs;
    Config* config;
  
    int topN;
    bool detectRegion;
    std::string defaultRegion;
    
  private:
    
    int threadCounter;
    tthread::mutex mMutex;
    cv::Mat* frame;
    vector<Rect> plateRegions;
    vector<AlprResult> recognitionResults;

};

#endif // ALPRIMPL_H