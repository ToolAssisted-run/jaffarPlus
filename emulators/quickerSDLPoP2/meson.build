quickerSDLPoP2Subproject = subproject('quickerSDLPoP2')

# Getting the quickerSDLPoP2 dependency (a single header and its JaffarPlus instance)
quickerSDLPoP2Dependency = quickerSDLPoP2Subproject.get_variable('quickerSDLPoP2Dependency')
emulatorDependencies += quickerSDLPoP2Dependency

# SDLPoP2 itself, for the player's drawing (its window through SDL2)
emulatorDependencies += dependency('sdl2')
emulatorDependencies += quickerSDLPoP2Subproject.get_variable('sdlpop2Dependency')
