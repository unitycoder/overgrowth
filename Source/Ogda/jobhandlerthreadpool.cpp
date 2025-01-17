//-----------------------------------------------------------------------------
//           Name: jobhandlerthreadpool.cpp
//      Developer: Wolfire Games LLC
//    Description:
//        License: Read below
//-----------------------------------------------------------------------------
//
//   Copyright 2022 Wolfire Games LLC
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//-----------------------------------------------------------------------------
#include <Memory/allocation.h>
#include "jobhandlerthreadpool.h"

#include <Logging/logdata.h>
#include <cstdlib>
#include <pthread.h>
#include <iostream>
#include "main.h"

#ifdef PLATFORM_LINUX
#include <unistd.h>
#endif

struct ItemJob
{
    Item* item;
    int step;
    int count;
    volatile int performed;
};

static void* CalculateItemHash( void* item )
{
    ItemJob* ij = static_cast<ItemJob*>(item); 
    ij->performed = 0;
    for( int i = 0; i < ij->count; i++ )
    {
        ij->item[i*ij->step].CalculateHash();
        ij->performed++;
    }
    return 0;
}

JobHandlerThreadPool::JobHandlerThreadPool( int thread_count ) : thread_count(thread_count)
{
    threads = static_cast<pthread_t*>(OG_MALLOC( sizeof(pthread_t) * thread_count ));
}

JobHandlerThreadPool::~JobHandlerThreadPool()
{
    OG_FREE(threads);
}

void JobHandlerThreadPool::RunHashCalculation(std::vector<Item>& arr)
{
    ItemJob* itemjobs = static_cast<ItemJob*>(OG_MALLOC(sizeof(ItemJob)*thread_count));
    
    int step = arr.size()/thread_count;

    for( int i = 0; i < thread_count; i++ )
    {
        itemjobs[i].item = &arr[i];
        itemjobs[i].step = thread_count;
        itemjobs[i].count = step;

        int err = pthread_create( &threads[i], NULL, CalculateItemHash, &itemjobs[i]);

        if( err != 0 )
        {
            LOGE << "Thread create failed" << std::endl;
        }
    }

    //Do the remaining items in our main thread.
    for( int i = step*thread_count; i < arr.size(); i++ )
    {
        arr[i].CalculateHash();
    }

    int count = 0;
    while( count < arr.size() )
    {
#ifdef PLATFORM_LINUX
        sleep(1);
#endif
        std::stringstream ss;
        count = arr.size()-step*thread_count;
        for( int i = 0; i < thread_count; i++ )
        {
            count += itemjobs[i].performed;
            ss << ((itemjobs[i].performed == itemjobs[i].count) ? "d" : ".");
        }
        SetPercent(ss.str().c_str(), (int)(100.0f*((float)count/(float)arr.size())));
    }

    for( int i = 0; i < thread_count; i++ )
    {
        void* retval; 
        int err = pthread_join( threads[i], &retval );

        if( err != 0 )
        {
            LOGE << "Thread join failed" << std::endl;
        }
    }
    OG_FREE( itemjobs );
}
