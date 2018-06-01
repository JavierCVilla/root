// @(#)root/tmva/tmva/dnn:$Id$
// Author: Simon Pfreundschuh 21/07/16

/*************************************************************************
 * Copyright (C) 2016, Simon Pfreundschuh                                *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

///////////////////////////////////////////////////////////////////////
// Implementation of the regularization functionals and gradients    //
// for the multi-threaded CPU implementation using Roots TThreadExecutor. //
///////////////////////////////////////////////////////////////////////

#include "TMVA/DNN/Architectures/Reference.h"

namespace TMVA
{
namespace DNN
{

//______________________________________________________________________________
template<typename AFloat>
AFloat TCpu<AFloat>::L1Regularization(const TCpuMatrix<AFloat> &Weights)
{
   const AFloat  *data = Weights.GetRawDataPointer();

   size_t nElements =  Weights.GetNoElements();
   size_t nSteps = TCpuMatrix<AFloat>::GetNWorkItems(nElements);

   std::vector<AFloat> temp(nElements/nSteps + 1);

   auto f = [&data, &temp, nElements, nSteps](UInt_t workerID)
   {
      size_t iMax = std::min(workerID+nSteps, nElements);
      size_t iWorker = workerID/nSteps; 
      for (size_t i = workerID; i < iMax; ++i) {
         temp[iWorker] += fabs(data[i]);
      }
   };

   auto reduction = [](const std::vector<AFloat> & v )
   {
      return std::accumulate(v.begin(),v.end(),AFloat{});
   };
   // auto reduction = [](AFloat sum1, AFloat sum2)
   // {
   //    return sum1 + sum2;
   // };

   Weights.GetThreadExecutor().Foreach(f, ROOT::TSeqI(0,nElements,nSteps) ); 
   return Weights.GetThreadExecutor().Reduce(temp, reduction);
}


//______________________________________________________________________________
template<typename AFloat>
void TCpu<AFloat>::AddL1RegularizationGradients(
    TCpuMatrix<AFloat> & B,
    const TCpuMatrix<AFloat> & A,
    AFloat weightDecay)
{
         AFloat  *dataB     =  B.GetRawDataPointer();
   const AFloat  *dataA      = A.GetRawDataPointer();

   size_t nElements =  B.GetNoElements();
   R__ASSERT(A.GetNoElements() == nElements);
   size_t nSteps = TCpuMatrix<AFloat>::GetNWorkItems(nElements);
   


   auto f = [&dataA, &dataB, weightDecay, nElements, nSteps](UInt_t workerID)
   {
      size_t iMax = std::min(workerID+nSteps, nElements);
      for (size_t i = workerID; i < iMax; ++i) {
         AFloat sign = (dataA[i] < 0.0) ? -1.0 : 1.0;
         dataB[i] += weightDecay * sign;
      }
      return 0;
   };

   B.GetThreadExecutor().Foreach(f, ROOT::TSeqI(0,nElements, nSteps));
}

//______________________________________________________________________________
template<typename AFloat>
AFloat TCpu<AFloat>::L2Regularization(const TCpuMatrix<AFloat> &Weights)
{
   const AFloat  *data = Weights.GetRawDataPointer();

   size_t nElements =  Weights.GetNoElements();
   size_t nSteps = TCpuMatrix<AFloat>::GetNWorkItems(nElements);

   std::vector<AFloat> temp(nElements/nSteps + 1);

   auto f = [&data, &temp, nElements, nSteps](UInt_t workerID)
   {
      size_t iMax = std::min(workerID+nSteps, nElements);
      size_t iWorker = workerID/nSteps;

      for (size_t i = workerID; i < iMax; ++i) {
         temp[iWorker] += data[i] * data[i];
      }
   };

   auto reduction = [](const std::vector<AFloat> & v )
   {
      return std::accumulate(v.begin(),v.end(),AFloat{});
   };
   // auto reduction = [](AFloat sum1, AFloat sum2)
   // {
   //    return sum1 + sum2;
   // };

   Weights.GetThreadExecutor().Foreach(f, ROOT::TSeqI(0,nElements,nSteps) ); 
   return Weights.GetThreadExecutor().Reduce(temp, reduction);
}

//______________________________________________________________________________
template<typename AFloat>
void TCpu<AFloat>::AddL2RegularizationGradients(
    TCpuMatrix<AFloat> & B,
    const TCpuMatrix<AFloat> & A,
    AFloat weightDecay)
{
         AFloat  *dataB     =  B.GetRawDataPointer();
   const AFloat  *dataA      = A.GetRawDataPointer();

      size_t nElements =  B.GetNoElements();
   R__ASSERT(A.GetNoElements() == nElements);
   size_t nSteps = TCpuMatrix<AFloat>::GetNWorkItems(nElements);

   auto f = [&dataA, &dataB, weightDecay, nElements, nSteps](UInt_t workerID)
   {
      size_t iMax = std::min(workerID+nSteps, nElements);
      for (size_t i = workerID; i < iMax; ++i) {
         dataB[i] += 2.0 * weightDecay * dataA[i];
      }
      return 0;
   };

   B.GetThreadExecutor().Foreach(f, ROOT::TSeqI(0,nElements, nSteps));
}

#if 0  // old implementation (to be removed) 
//______________________________________________________________________________
template<typename AFloat>
AFloat TCpu<AFloat>::L1Regularization(const TCpuMatrix<AFloat> &Weights)
{
   const AFloat  *data = Weights.GetRawDataPointer();
   std::vector<AFloat> temp(Weights.GetNElements());

   auto f = [&data, &temp](UInt_t workerID)
   {
      temp[workerID] = fabs(data[workerID]);
      return 0;
   };

   auto reduction = [](const std::vector<AFloat> & v )
   {
      return std::accumulate(v.begin(),v.end(),AFloat{});
   };
   // auto reduction = [](AFloat sum1, AFloat sum2)
   // {
   //    return sum1 + sum2;
   // };

   Weights.GetThreadExecutor().Map(f, ROOT::TSeqI(Weights.GetNoElements()));
   return Weights.GetThreadExecutor().Reduce(temp, reduction);
}


//______________________________________________________________________________
template<typename AFloat>
void TCpu<AFloat>::AddL1RegularizationGradients(
    TCpuMatrix<AFloat> & B,
    const TCpuMatrix<AFloat> & A,
    AFloat weightDecay)
{
         AFloat  *dataB     =  B.GetRawDataPointer();
   const AFloat  *dataA      = A.GetRawDataPointer();

   auto f = [&dataA, &dataB, weightDecay](UInt_t workerID)
   {
      AFloat sign = (dataA[workerID] < 0.0) ? -1.0 : 1.0;
      dataB[workerID] += weightDecay * sign;
      return 0;
   };

   B.GetThreadExecutor().Map(f, ROOT::TSeqI(B.GetNoElements()));
}

//______________________________________________________________________________
template<typename AFloat>
AFloat TCpu<AFloat>::L2Regularization(const TCpuMatrix<AFloat> &Weights)
{
   const AFloat  *data = Weights.GetRawDataPointer();
   std::vector<AFloat> temp(Weights.GetNoElements());

   auto f = [&data, &temp](UInt_t workerID)
   {
      temp[workerID] = data[workerID] * data[workerID];
      return 0;
   };

   auto reduction = [](const std::vector<AFloat> & v )
   {
      return std::accumulate(v.begin(),v.end(),AFloat{});
   };
   // auto reduction = [](AFloat sum1, AFloat sum2)
   // {
   //    return sum1 + sum2;
   // };

   Weights.GetThreadExecutor().Map(f, ROOT::TSeqI(Weights.GetNoElements()));
   return Weights.GetThreadExecutor().Reduce(temp, reduction);
}

//______________________________________________________________________________
template<typename AFloat>
void TCpu<AFloat>::AddL2RegularizationGradients(
    TCpuMatrix<AFloat> & B,
    const TCpuMatrix<AFloat> & A,
    AFloat weightDecay)
{
         AFloat  *dataB     =  B.GetRawDataPointer();
   const AFloat  *dataA      = A.GetRawDataPointer();

   auto f = [&dataA, &dataB, weightDecay](UInt_t workerID)
   {
      dataB[workerID] += 2.0 * weightDecay * dataA[workerID];
      return 0;
   };

   B.GetThreadExecutor().Map(f, ROOT::TSeqI(B.GetNoElements()));
}

#endif

} // namespace DNN
} // namespace TMVA
