#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>

using namespace geode::prelude;

struct TriggerData {
    EffectGameObject* trigger;
    int targetGroup = 0;
    int centerGroup = 0;

    TriggerData(EffectGameObject* obj, int target, int center)
        : trigger(obj), targetGroup(target), centerGroup(center) {}
};

std::unordered_map<int, int> groupParentObjs;

auto mod = Mod::get();

bool copyParent = mod->getSavedValue<bool>("copy-parent-enabled");

class $modify(Editor, EditorUI) {
    gd::string copyObjects(CCArray* objects, bool copyColors, bool sort) {
        // ok so this entire thing is built on the assumption that the copy gdstring builds based of the order of the inputted ccarray
        // i have no idea why this wouldnt be the case and in testing it works like that but this is also rly sketchy, idk if it works it works
        groupParentObjs.clear();
        auto parentDict = m_editorLayer->m_parentGroupsDict;
        for (auto key : CCArrayExt<CCInteger*>(parentDict->allKeys())) {
            auto value = key->getValue(); // allkeys returns ccint for some ungodly reason even tho objectforkey takes int not ccint why cocos
            auto index = objects->indexOfObject(parentDict->objectForKey(value)); // fuck null checking robtop got me
            if (index != UINT_MAX) groupParentObjs.insert({value, index}); // fuck null checking indexofobject returning uint_max on null got me
        }
        return EditorUI::copyObjects(objects, copyColors, sort);
    }

    CCArray* pasteObjects(gd::string p0, bool p1, bool p2) {
        CCArray* ret = EditorUI::pasteObjects(p0, p1, p2);
        
        if (groupParentObjs.empty()) return ret;
        if (copyParent) {
            auto editor = m_editorLayer;
            for (auto [groupParent, index] : groupParentObjs) {
                editor->setGroupParent(static_cast<GameObject*>(ret->objectAtIndex(index)), groupParent);
            }
        }
        return ret;
    }
    
    void pasteBuildHelper() {
        copyParent = false;
        auto objs = pasteObjects(copyObjects(getSelectedObjects(), false, false), false, false);
        copyParent = mod->getSavedValue<bool>("copy-parent-enabled");
        buildHelperExtra(objs, true);
        this->updateButtons();
        this->updateObjectInfoLabel();
    }

    void buildHelperExtra(CCArray* array, bool updateParents = false) { // not compatible with a few edge cases but noone uses those fuckass triggers anyway (totally)
        std::set<int> triggerGroups;
        std::vector<TriggerData> triggers;
        auto objs = ccArrayToVector<GameObject*>(array);
        for (auto obj : objs) {
            if (auto trigger = typeinfo_cast<EffectGameObject*>(obj)) {
                auto target = trigger->m_targetGroupID;
                auto center = trigger->m_centerGroupID;
                
                triggerGroups.insert(target);
                triggerGroups.insert(center);
                triggers.emplace_back(trigger, target, center);
            }
        }
        triggerGroups.erase(0);
        if (triggerGroups.empty()) return;

        std::unordered_map<int, std::vector<GameObject*>> objectGroupMap;
        for (auto obj : objs) {
            auto groups = obj->m_groups;
            if (!groups) continue;
            for (int group : *groups) {
                if (group == 0) break; // if 0 shows up that means all valid groups finished or there just isnt any groups :3
                if (triggerGroups.contains(group)) objectGroupMap[group].push_back(obj);
            }
        }

        auto editor = m_editorLayer;
        for (auto triggerGroup : triggerGroups) { // mb this is optimised badly but its not like its running every frame so its fine
            if (!objectGroupMap.contains(triggerGroup)) continue;
            auto freeGroup = editor->getNextFreeGroupID(editor->m_objects);

            GameObject* groupParent = (groupParentObjs.contains(triggerGroup) && updateParents) ?
            static_cast<GameObject*>(array->objectAtIndex(groupParentObjs.at(triggerGroup))) : nullptr;

            for (auto obj : objectGroupMap.at(triggerGroup)) {
                obj->removeFromGroup(triggerGroup);
                obj->addToGroup(freeGroup);
                if (groupParent && groupParent == obj) editor->setGroupParent(obj, freeGroup); // fuck null checking && got me
            }

            for (auto [trigger, target, center] : triggers) {
                if (target == triggerGroup) {
                    trigger->m_targetGroupID = freeGroup;
                    if (auto label = trigger->getObjectLabel()) label->setString(std::to_string(freeGroup).c_str()); // update label (copied from trigger context menus)
                }
                if (center == triggerGroup) {
                    trigger->m_centerGroupID = freeGroup;
                }
            }
        }
    }
};

class $modify(EditorPause, EditorPauseLayer) {
    bool init (LevelEditorLayer* p0) {
        if (!EditorPauseLayer::init(p0)) return false;

        auto menu = this->getChildByID("guidelines-menu");
        if (!menu) return true;

        auto toggler = CCMenuItemToggler::create(CircleButtonSprite::create(CCSprite::create("togglebuttonoff.png"_spr), CircleBaseColor::Pink, CircleBaseSize::Small), 
        CircleButtonSprite::create(CCSprite::create("togglebuttonon.png"_spr), CircleBaseColor::Pink, CircleBaseSize::Small), this, menu_selector(EditorPause::onToggleCopyParent));
        toggler->toggle(copyParent);

        auto pasteWithBuildhelper = CCMenuItemSpriteExtra::create(CircleButtonSprite::create(
        CCSprite::create("buildhelperpaste.png"_spr), CircleBaseColor::Cyan, CircleBaseSize::Small), this, menu_selector(EditorPause::onPasteBuildHelper));

        menu->addChild(toggler);
        menu->addChild(pasteWithBuildhelper);
        menu->updateLayout();

        return true;
    }

    void onToggleCopyParent(CCObject* sender) {
        mod->setSavedValue<bool>("copy-parent-enabled", !copyParent);
        copyParent = !copyParent;;
    }

    void onPasteBuildHelper(CCObject* sender) {
        static_cast<Editor*>(EditorUI::get())->pasteBuildHelper();
    }
};