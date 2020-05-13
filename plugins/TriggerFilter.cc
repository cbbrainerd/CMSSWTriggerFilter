// -*- C++ -*-
//
// Package:    HiggsAnalysis/TriggerFilter
// Class:      TriggerFilter
// 
/**\class TriggerFilter TriggerFilter.cc HiggsAnalysis/TriggerFilter/plugins/TriggerFilter.cc

 Description: [one line class summary]

 Implementation:
     [Notes on implementation]
*/
//
// Original Author:  Christopher Brainerd
//         Created:  Tue, 03 Mar 2020 20:37:40 GMT
//
//

// system include files
#include <memory>

// user include files
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/stream/EDFilter.h"

#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/Utilities/interface/StreamID.h"

#include "FWCore/Common/interface/TriggerNames.h"
#include "DataFormats/Common/interface/TriggerResults.h"

#include "FWCore/MessageLogger/interface/MessageLogger.h"

#include "FWCore/Utilities/interface/EDPutToken.h"

#include <algorithm>
//
// class declaration
//

template <class Key,class... Args>
auto search_for_prefix(std::map<Key,Args...>& map_to_search,const Key& search_key) -> decltype(map_to_search.upper_bound(search_key)) {
    //Find the first trigger that is greater than the one being searched for
    auto ub=map_to_search.upper_bound(search_key);
    //If the first element is already greater than the string being searched for, give up
    if(ub==map_to_search.begin()) return map_to_search.end();
    //Go back one for the last trigger that is not greater than the one being searched for
    const std::string &found_string=(--ub)->first;
    //Check if that trigger is a prefix of the search string
    //Check that it has a size that is less than or equal and that there is no mismatch
    //If so, return this element
    if(found_string.size() <= search_key.size() && std::mismatch(found_string.begin(),found_string.end(),search_key.begin()).first==found_string.end()) return ub;
    //Else, return last_prefix
    else return map_to_search.end();
}

class TriggerFilter : public edm::stream::EDFilter<> {
   public:
      explicit TriggerFilter(const edm::ParameterSet&);
      ~TriggerFilter();

      static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

   private:
      virtual void beginStream(edm::StreamID) override;
      virtual bool filter(edm::Event&, const edm::EventSetup&) override;
      virtual void endStream() override;

      virtual void setup_trigger(const edm::TriggerNames&);

      enum trigger_type { 
          pass,
          veto,
          ignore
      };
    
      static inline const char* trigger_type_name(const trigger_type& tt) {
          switch(tt) {
              case pass:
                return "pass";
              case veto:
                return "veto";
              case ignore:
                return "ignored";
          }
          //Compiler complains if we don't have something
          return "?!?!?";
      }

      edm::EDGetTokenT<edm::TriggerResults> trigger_results;
      edm::ParameterSetID trigger_names_id;
      //The trigger paths that should be checked. Saved as a tuple: the name of the trigger, an enum indicating whether this is a pass trigger, veto trigger, or ignored trigger, and an integer indicating the index of the trigger
      std::map<std::string,std::pair<trigger_type,unsigned int> > triggers;
      //Contains a list of indices for pass/veto triggers
      //First element of tuple is true for veto, second element is index of the matched trigger in the previous std::map, and the third element is the actual index of the trigger in the trigger results
      std::vector<std::tuple<trigger_type,unsigned int,unsigned int> > trigger_indices;

      //Bitmask for matched triggers. 
      edm::EDPutTokenT<std::vector<bool> > triggers_fired_token;
      //virtual void beginRun(edm::Run const&, edm::EventSetup const&) override;
      //virtual void endRun(edm::Run const&, edm::EventSetup const&) override;
      //virtual void beginLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&) override;
      //virtual void endLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&) override;

      // ----------member data ---------------------------
};

//
// constants, enums and typedefs
//

//
// static data member definitions
//

//
// constructors and destructor
//
TriggerFilter::TriggerFilter(const edm::ParameterSet& iConfig) : 
    trigger_results(consumes<edm::TriggerResults>(iConfig.getParameter<edm::InputTag>("trigger_results"))),
    trigger_names_id(),
    triggers(),
    trigger_indices(),
    triggers_fired_token(produces<std::vector<bool> >("triggersFired"))
{
   auto log=edm::LogInfo("HLTFilter");
   log << "SETTING UP TRIGGER FILTER." << std::endl;
   //Fill passing and failing triggers. These will be alphabetized by std::set
   for(auto const &pass : iConfig.getParameter<std::vector<std::string> >("pass_triggers")) {
      triggers.insert(std::make_pair(pass,std::make_pair(trigger_type::pass,0)));
   }
   for(auto const &veto : iConfig.getParameter<std::vector<std::string> >("veto_triggers")) {
      triggers.insert(std::make_pair(veto,std::make_pair(trigger_type::veto,0)));
   }
   for(auto const &ignore : iConfig.getParameter<std::vector<std::string> >("ignore_triggers")) {
      triggers.insert(std::make_pair(ignore,std::make_pair(trigger_type::ignore,0)));
   }
   //Give each trigger an index. These should be the same for every job since all triggers are sorted in one list regardless of pass/veto
   unsigned int i=0;
   for(auto x=triggers.begin();x!=triggers.end();++i,++x) {
       x->second.second=i;
   }
   log << "Triggers:\nIndex Trigger Type\n";
   std::size_t index=0;
   for(auto x=triggers.begin();x!=triggers.end();++index,++x) {
       log << index << " " << x->first << " " << x->second.second << " " << trigger_type_name(x->second.first) << "\n";
   }
}


TriggerFilter::~TriggerFilter()
{
 
   // do anything here that needs to be done at destruction time
   // (e.g. close files, deallocate resources etc.)

}


//
// member functions
//

//Called each time the HLT trigger names change
void
TriggerFilter::setup_trigger(const edm::TriggerNames& trigger_names) {
    trigger_indices.clear();
    auto log=edm::LogInfo("HLTFilter");
    log << "Trigger names changed\n";
    for(unsigned int i=0;i!=trigger_names.size();++i) {
        const std::string& name=trigger_names.triggerName(i);
        auto matched_trigger=search_for_prefix(triggers,name);
        if(matched_trigger==triggers.end()) log << "Trigger \"" << name << "\" did not match any pass or veto trigger.\n";
        else {
            const std::string& trigger_name=matched_trigger->first;
            const trigger_type& tt=matched_trigger->second.first;
            const char* tt_name=trigger_type_name(tt);
            const unsigned int& index=matched_trigger->second.second;
            trigger_indices.push_back(std::make_tuple(tt,index,i));
            log << "Trigger \"" << name << "\" matched " << tt_name << " trigger \"" << trigger_name << "\" at index " << index << "\n";
        }
    }
}

// ------------ method called on each new Event  ------------
bool
TriggerFilter::filter(edm::Event& iEvent, const edm::EventSetup& iSetup)
{
   std::unique_ptr<std::vector<bool> > triggers_fired(new std::vector<bool>);
   triggers_fired->resize(triggers.size());
   edm::Handle<edm::TriggerResults> h_trigger_results;
   iEvent.getByToken(trigger_results,h_trigger_results);
   if(!h_trigger_results.isValid()) { throw cms::Exception("ProductNotValid") << "TriggerResults product not valid"; }
   const edm::TriggerNames& trigger_names=iEvent.triggerNames(*h_trigger_results);
   //Check if the trigger menu has changed since the last event was processed
   auto const& new_names=trigger_names.parameterSetID();
   if(new_names != trigger_names_id) {
       trigger_names_id=new_names;
       setup_trigger(trigger_names);
   }
   bool trigger_passed=false;
   for(auto const& trigger_check : trigger_indices) {
       unsigned int check_index=std::get<2>(trigger_check);
       if(h_trigger_results->accept(check_index)) {
           //If a veto trigger was accepted, reject the event immediately
           trigger_type tt=std::get<0>(trigger_check);
           switch(tt) {
               case veto:
                   return false;
                   //As long as there is no veto, this should pass the trigger
               case pass:
                   trigger_passed=true;
               case ignore:
                   //Record the trigger decision (in both pass and ignore case)
                   (*triggers_fired)[std::get<1>(trigger_check)] = true;
           }
       }
   }
   iEvent.put(triggers_fired_token,std::move(triggers_fired));
   return trigger_passed;
}

// ------------ method called once each stream before processing any runs, lumis or events  ------------
void
TriggerFilter::beginStream(edm::StreamID)
{
}

// ------------ method called once each stream after processing all runs, lumis and events  ------------
void
TriggerFilter::endStream() {
}

// ------------ method called when starting to processes a run  ------------
/*
void
TriggerFilter::beginRun(edm::Run const&, edm::EventSetup const&)
{ 
}
*/
 
// ------------ method called when ending the processing of a run  ------------
/*
void
TriggerFilter::endRun(edm::Run const&, edm::EventSetup const&)
{
}
*/
 
// ------------ method called when starting to processes a luminosity block  ------------
/*
void
TriggerFilter::beginLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&)
{
}
*/
 
// ------------ method called when ending the processing of a luminosity block  ------------
/*
void
TriggerFilter::endLuminosityBlock(edm::LuminosityBlock const&, edm::EventSetup const&)
{
}
*/
 
// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void
TriggerFilter::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
  edm::ParameterSetDescription desc;
  desc.add<edm::InputTag>("trigger_results",edm::InputTag("TriggerResults","","HLT"));
  desc.add<std::vector<std::string> >("pass_triggers");
  desc.add<std::vector<std::string> >("veto_triggers");
  desc.add<std::vector<std::string> >("ignore_triggers");
  descriptions.addDefault(desc);
}
//define this as a plug-in
DEFINE_FWK_MODULE(TriggerFilter);
