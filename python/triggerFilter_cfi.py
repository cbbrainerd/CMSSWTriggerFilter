import FWCore.ParameterSet.Config as cms

from triggerMenus_cff import get_triggers

def get_trigger_filter(year,isMC,dataset):
    print "Setting up trigger for year %i on dataset %s (%s)" % (year,dataset,'MC' if isMC else 'data')
    if isMC:
        dataset='MC'
    triggers=get_triggers(year,dataset)
    print "Triggering on events that pass the triggers:"
    for trigger in triggers['pass_triggers']:
        print trigger
    veto=triggers['veto_triggers']
    if veto:
        print "Except those that pass the triggers:"
        for trigger in veto:
            print trigger
    else:
        print "No veto triggers."
    ignore=triggers['ignore_triggers']
    if ignore:
        print 'The following triggers are ignored, but the trigger decision will be saved:'
        for trigger in ignore:
            print trigger
    triggerFilter=cms.EDFilter("TriggerFilter",
        trigger_results=cms.InputTag("TriggerResults","","HLT"),
        pass_triggers=cms.vstring(triggers['pass_triggers']),
        veto_triggers=cms.vstring(triggers['veto_triggers']),
        ignore_triggers=cms.vstring(triggers['ignore_triggers'])
    )
    print "Trigger configuration done."
    return triggerFilter
