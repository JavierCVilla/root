// Author: Enrico Guiraud, Danilo Piparo CERN  09/2018

/*************************************************************************
 * Copyright (C) 1995-2016, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include "ROOT/RCutFlowReport.hxx"
#include "ROOT/RDFBookedCustomColumns.hxx"
#include "ROOT/RLoopManager.hxx"
#include "ROOT/RJittedFilter.hxx"

using namespace ROOT::Detail::RDF;

RJittedFilter::RJittedFilter(RLoopManager *lm, std::string_view name)
   : RFilterBase(lm, name, lm->GetNSlots(), RDFInternal::RBookedCustomColumns()) { }

void RJittedFilter::SetFilter(std::unique_ptr<RFilterBase> f)
{
   fConcreteFilter = std::move(f);
}

void RJittedFilter::InitSlot(TTreeReader *r, unsigned int slot)
{
   R__ASSERT(fConcreteFilter != nullptr);
   fConcreteFilter->InitSlot(r, slot);
}

bool RJittedFilter::CheckFilters(unsigned int slot, Long64_t entry)
{
   R__ASSERT(fConcreteFilter != nullptr);
   return fConcreteFilter->CheckFilters(slot, entry);
}

void RJittedFilter::Report(ROOT::RDF::RCutFlowReport &cr) const
{
   R__ASSERT(fConcreteFilter != nullptr);
   fConcreteFilter->Report(cr);
}

void RJittedFilter::PartialReport(ROOT::RDF::RCutFlowReport &cr) const
{
   R__ASSERT(fConcreteFilter != nullptr);
   fConcreteFilter->PartialReport(cr);
}

void RJittedFilter::FillReport(ROOT::RDF::RCutFlowReport &cr) const
{
   R__ASSERT(fConcreteFilter != nullptr);
   fConcreteFilter->FillReport(cr);
}

void RJittedFilter::IncrChildrenCount()
{
   R__ASSERT(fConcreteFilter != nullptr);
   fConcreteFilter->IncrChildrenCount();
}

void RJittedFilter::StopProcessing()
{
   R__ASSERT(fConcreteFilter != nullptr);
   fConcreteFilter->StopProcessing();
}

void RJittedFilter::ResetChildrenCount()
{
   R__ASSERT(fConcreteFilter != nullptr);
   fConcreteFilter->ResetChildrenCount();
}

void RJittedFilter::TriggerChildrenCount()
{
   R__ASSERT(fConcreteFilter != nullptr);
   fConcreteFilter->TriggerChildrenCount();
}

void RJittedFilter::ResetReportCount()
{
   R__ASSERT(fConcreteFilter != nullptr);
   fConcreteFilter->ResetReportCount();
}

void RJittedFilter::ClearValueReaders(unsigned int slot)
{
   R__ASSERT(fConcreteFilter != nullptr);
   fConcreteFilter->ClearValueReaders(slot);
}

void RJittedFilter::ClearTask(unsigned int slot)
{
   R__ASSERT(fConcreteFilter != nullptr);
   fConcreteFilter->ClearTask(slot);
}

void RJittedFilter::InitNode()
{
   R__ASSERT(fConcreteFilter != nullptr);
   fConcreteFilter->InitNode();
}

void RJittedFilter::AddFilterName(std::vector<std::string> &filters)
{
   if (fConcreteFilter == nullptr) {
      // No event loop performed yet, but the JITTING must be performed.
      GetLoopManagerUnchecked()->BuildJittedNodes();
   }
   fConcreteFilter->AddFilterName(filters);
}

std::shared_ptr<RDFGraphDrawing::GraphNode> RJittedFilter::GetGraph()
{
   if (fConcreteFilter != nullptr) {
      // Here the filter exists, so it can be served
      return fConcreteFilter->GetGraph();
   }
   throw std::runtime_error("The Jitting should have been invoked before this method.");
}

