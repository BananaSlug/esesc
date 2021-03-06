#if 0
// Contributed by Jose Renau
//                Basilio Fraguela
//
// The ESESC/BSD License
//
// Copyright (c) 2005-2013, Regents of the University of California and 
// the ESESC Project.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   - Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
//
//   - Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
//
//   - Neither the name of the University of California, Santa Cruz nor the
//   names of its contributors may be used to endorse or promote products
//   derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#ifndef SP_H 
#define SP_H

#include "GStats.h"
#include "Port.h"
#include "MemRequest.h"
#include "MemObj.h"
//#include "DDR2.h"

#include <queue>
#include <vector>
#include <deque>
#include <stack>
#include <map>
#include "callback.h"
#include "estl.h"
#include "CacheCore.h"


class BState : public StateGeneric<AddrType> {
  public:
    BState(int32_t lineSize) {
    }
};

class PfState : public StateGeneric <AddrType> {
  public:
    PfState(int32_t lineSize) {
    }

    uint32_t stride;
    bool goingUp;

    AddrType nextAddr(CacheGeneric<PfState,AddrType> *c) {
      AddrType preaddr = c->calcAddr4Tag(getTag());
      return (goingUp ? (preaddr + stride) : (preaddr - stride));
    }
};


class StridePrefetcher: public MemObj {

private:
  typedef CacheGeneric<BState,AddrType> BuffType;
  typedef CacheGeneric<BState,AddrType>::CacheLine bLine;

  typedef CacheGeneric<PfState,AddrType> PfTable;
  typedef CacheGeneric<PfState,AddrType>::CacheLine pEntry;


  struct hash_long_long {
    size_t operator()(const AddrType in)  const {
      //uint32_t ret = (in >> 32L) ^ (in & 0xFFFFFFFF);
      //return (uint32_t) ret;
      return (uint32_t) in;
    }
  };

  class AddrTypeHashFunc {
    public:
      size_t operator()(const AddrType p) const {
        return((int) p);
      }
  };

  class AddrTypeEqual {
    public:
      bool operator()(const AddrType &x, const AddrType &y) const{
        return (memcmp((const void*)x, (const void*)y, sizeof(AddrType)) == 0);
      }
  };
  
  typedef HASH_MAP<AddrType, std::queue<MemRequest *>, AddrTypeHashFunc> penReqMapper;
  typedef HASH_SET<AddrType, AddrTypeHashFunc> penFetchSet;


  uint32_t pendingRequests;
  uint32_t pendingFetches;


  BuffType *buff;
  PfTable  *table;

  std::deque<AddrType> lastMissesQ;

  PortGeneric *buffPort;
  PortGeneric *tablePort;

  int32_t numStreams;
  int32_t streamAssoc;
  int32_t depth;
  int32_t numBuffPorts;
  int32_t buffPortOccp;
  int32_t numTablePorts;
  int32_t tablePortOccp;
  int32_t hitDelay;
  int32_t missDelay;
  int32_t learnHitDelay;
  int32_t learnMissDelay;
  uint32_t missWindow;
  uint32_t maxStride;
  uint32_t MaxPendingRequests;
  static const int32_t pEntrySize = 8; // size of an entry in the prefetching table
  
  //int32_t defaultMask;
  AddrType defaultMask;

  GStatsCntr halfMiss;
  GStatsCntr miss;
  GStatsCntr hit;
  GStatsCntr predictions;
  GStatsCntr accesses;
  GStatsCntr unitStrideStreams;
  GStatsCntr nonUnitStrideStreams;
  GStatsCntr ignoredStreams;

protected:
  TimeDelta_t delay;
//  PortGeneric *dataPort;
//  PortGeneric *cmdPort;

  bool isMemoryBus;
//  DDR2 **DRAM;
//  int numChannels;

public:
  StridePrefetcher(MemorySystem* current, const char *device_descr_section, const char *device_name = NULL);
  ~StridePrefetcher() {}

  Time_t nextReadSlot(       const MemRequest *mreq);
  Time_t nextWriteSlot(      const MemRequest *mreq);
  Time_t nextBusReadSlot(    const MemRequest *mreq);
  Time_t nextPushDownSlot(   const MemRequest *mreq);
  Time_t nextPushUpSlot(     const MemRequest *mreq);
  Time_t nextInvalidateSlot( const MemRequest *mreq);

  // processor direct requests
  void read(MemRequest  *req);
  void write(MemRequest *req);
  void writeAddress(MemRequest *req);

  // DOWN
  void busRead(MemRequest *req);
  void busReadAck(MemRequest *mreq);
  void pushDown(MemRequest *req);
  
  // UP
  void pushUp(MemRequest *req);
  void invalidate(MemRequest *req);

  // Status/state
  uint16_t getLineSize() const;

  bool canAcceptRead(DInst *dinst) const;
  bool canAcceptWrite(DInst *dinst) const;

  TimeDelta_t ffread(AddrType addr, DataType data);
  TimeDelta_t ffwrite(AddrType addr, DataType data);
  void        ffinvalidate(AddrType addr, int32_t lineSize);

  //Added from the old prefetcher
  void learnHit(AddrType addr,MemRequest* orig_mreq);
  void learnMiss(AddrType addr, MemRequest* orig_mreq);
  void prefetch(pEntry *pe, Time_t lat, MemRequest* orig_mreq);
  void processAck(AddrType addr);

  typedef CallbackMember1<StridePrefetcher, AddrType, &StridePrefetcher::processAck> processAckCB;

  Time_t nextBuffSlot() {
    return buffPort->nextSlot();
  }

  Time_t nextTableSlot() {
    return tablePort->nextSlot();
  }
};

#endif

#endif
