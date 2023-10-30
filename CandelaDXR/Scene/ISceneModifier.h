#pragma once

namespace candela::scene
{
    class ISceneModifier
    {
    public:
        virtual ~ISceneModifier() = default;

        // Applies modifiers to scene materials, lights, etc
        virtual void modifyScene() = 0;
    };
}
