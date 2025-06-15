#pragma once
// idc if this is kinda cursed

struct TriggerData {
    EffectGameObject* trigger;
    int targetGroup = 0;
    int centerGroup = 0;

    TriggerData(EffectGameObject* obj, int target, int center)
        : trigger(obj), targetGroup(target), centerGroup(center) {}
};

inline std::unordered_map<int, int> groupParentObjs;
inline bool copyParent = false;
inline bool replaceBuildHelper = false;