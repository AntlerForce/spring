﻿// This file is part of the Spring engine (GPL v2 or later), see LICENSE.html

#pragma once

#include "plugin/SolLuaPlugin.h"
#include <RmlUi/Core.h>

namespace sol
{
    class state_view;
}

namespace Rml::SolLua
{
    /// <summary>
    /// Initializes RmlSolLua using the supplied Lua state.
    /// </summary>
    /// <param name="state">The Lua state to initialize into.</param>
    SolLuaPlugin* Initialise(sol::state_view* state);

    /// <summary>
    /// Initializes RmlSolLua using the supplied Lua state.
    /// Sets the Lua variable specified by lua_environment_identifier to the document's id when running Lua code.
    /// </summary>
    /// <param name="state">The Lua state to initialize into.</param>
    /// <param name="lua_environment_identifier">The Lua variable name that is set to the document's id.</param>
    SolLuaPlugin* Initialise(sol::state_view* state, const Rml::String& lua_environment_identifier);

    /// <summary>
    /// Registers RmlSolLua into the specified Lua state.
    /// </summary>
    /// <param name="state">The Lua state to register into.</param>
    void RegisterLua(sol::state_view* state, SolLuaPlugin* slp);

} // end namespace Rml::SolLua