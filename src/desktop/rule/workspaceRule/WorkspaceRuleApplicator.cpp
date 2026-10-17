#include "WorkspaceRuleApplicator.hpp"
#include "desktop/DesktopTypes.hpp"

using namespace Config;



// move the workspace rules logic from here to src/desktop/rule -- integrate it into the same framework that window rules has



// maybe get rid of props part - in building the merged rule it would actually be more work to check if prop matches than to just build the total rule and update the values to the new one

// need to move the merged rule building logic here -- if the rule passes the filter, apply it. no need a merged rule object




// move or remove files from src/config/shared/workspace and delete the folder --- workspace rules are to be moved into rules/








CWorkspaceRuleApplicator::CWorkspaceRuleApplicator(PHLWORKSPACE w) : m_workspace(w) {
    
    
}

void CWorkspaceRuleApplicator::propertiesChanged(std::underlying_type_t<eWorkspaceRule> props) {

    // Copy the current object and save it as state_OLD or something -- This is the old state

    // compute new state checking only what the props asks for (can we do this or does the checker just go through all the workspace rules?)

    // feed the old state into needsReLayout and let it find the diff -- fire the event and pass the old state to the event


}



void CWorkspaceRuleApplicator::resetProps(eWorkspaceRule prop) {


    // need to look into how window rules do it
 
    

}

bool CWorkspaceRuleApplicator::needsReLayout() {

}
