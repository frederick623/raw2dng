/* This file is part of the dngconvert project
   Copyright (C) 2011 Jens Mueller <tschensensinger at gmx dot de>
   
   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public   
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
   
   This file uses code from dng_threaded_host.cpp -- Sandy McGuffog CornerFix utility
   (http://sourceforge.net/projects/cornerfix, sandy dot cornerfix at gmail dot com),
   dng_threaded_host.cpp is copyright 2007-2011, by Sandy McGuffog and Contributors.
*/

#include "dnghost.h"
#include "dng_abort_sniffer.h"
#include "dng_area_task.h"
#include "dng_rect.h"

#include <thread>
#include <vector>
#include <exception>
#include "dng_sdk_limits.h"

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

static std::exception_ptr threadException = nullptr;

static void executeAreaThread(std::reference_wrapper<dng_area_task> task, uint32 threadIndex, const dng_rect &threadArea, const dng_point &tileSize, dng_abort_sniffer *sniffer) {
   try { task.get().ProcessOnThread(threadIndex, threadArea, tileSize, sniffer, NULL); }
   catch (...) { threadException = std::current_exception(); }
}

void DngHost::PerformAreaTask(dng_area_task &task, const dng_rect &area, dng_area_task_progress *progress) {
    dng_point tileSize(task.FindTileSize(area));

    uint32 vTilesinArea = area.H() / tileSize.v; if ((area.H() - (vTilesinArea * tileSize.v)) > 0) vTilesinArea++;
    uint32 hTilesinArea = area.W() / tileSize.h; if ((area.W() - (hTilesinArea * tileSize.h)) > 0) hTilesinArea++;

    uint32 maxThreads = Min_uint32(task.MaxThreads(), kMaxMPThreads);
    task.Start(maxThreads, area, tileSize, &Allocator(), Sniffer());

    threadException = nullptr;

    boost::asio::thread_pool pool(maxThreads);

    dng_rect threadArea(area.t, area.l, area.t + tileSize.v, area.l + tileSize.h);
    uint32 threadIndex = 0;

    for (uint32 vIndex = 0; vIndex < vTilesinArea; vIndex++) {
        for (uint32 hIndex = 0; hIndex < hTilesinArea; hIndex++) {
            threadArea.b = Min_int32(threadArea.t + tileSize.v, area.b);
            threadArea.r = Min_int32(threadArea.l + tileSize.h, area.r);

            uint32 assignedThreadIndex = threadIndex % maxThreads;

            boost::asio::post(pool, [t = std::ref(task), assignedThreadIndex, threadArea, tileSize, s = Sniffer()]() {
                executeAreaThread(t, assignedThreadIndex, threadArea, tileSize, s);
            });

            threadIndex++;
            threadArea.l = threadArea.r;
        }
        threadArea.t = threadArea.b;
        threadArea.l = area.l;
    }

    pool.join();

    if (threadException) std::rethrow_exception(threadException);

    task.Finish(maxThreads);
}
