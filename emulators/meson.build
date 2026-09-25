# Getting emulator from the user configuration
emulator = get_option('emulator')

if emulator == 'QuickerNES' or emulator == 'QuickerNESArkanoid'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERNES'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_NES'
subdir('quickerNES')

if emulator == 'QuickerNESArkanoid'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERNES_ARKANOID'
endif

endif

if emulator == 'QuickNES'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKNES'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_NES'
subdir('quickNES')
endif

if emulator == 'QuickerSDLPoP'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERSDLPOP'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_SDLPOP'
subdir('quickerSDLPoP')
endif

if emulator == 'QuickerSDLPoP2'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERSDLPOP2'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_SDLPOP2'
subdir('quickerSDLPoP2')
endif

if emulator == 'QuickerSnes9x'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERSNES9X'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_SNES'
subdir('quickerSnes9x')
endif

if emulator == 'QuickerGPGX'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERGPGX'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_GENESIS'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_SEGACD'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_SG1000'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_SMS'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_GAMEGEAR'
subdir('quickerGPGX')
endif

if emulator == 'QuickerStella'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERSTELLA'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_A2600'
subdir('quickerStella')
endif

if emulator == 'Atari2600Hawk'
jaffarCPPFlags += '-D__JAFFAR_USE_ATARI2600HAWK'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_A2600'
subdir('atari2600Hawk')
endif

if emulator == 'QuickerSMBC'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERSMBC'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_SMBC'
subdir('quickerSMBC')
endif

if emulator == 'QuickerNEORAW'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERNEORAW'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_NEORAW'
subdir('quickerNEORAW')
endif

if emulator == 'QuickerRAWGL'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERRAWGL'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_RAWGL'
subdir('quickerRAWGL')
endif

if emulator == 'QuickerArkBot'
jaffarCPPFlags += '-D__JAFFAR_USE_ARKBOT'
subdir('quickerArkBot')
endif

if emulator == 'QuickerMGBA'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERMGBA'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_GBA'
subdir('quickerMGBA')
endif

if emulator == 'QuickerGambatte'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERGAMBATTE'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_GB'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_GBC'
subdir('quickerGambatte')
endif

if emulator == 'QuickerDSDA'
jaffarCPPFlags += '-D__JAFFAR_USE_QUICKERDSDA'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_DOOM'
subdir('quickerDSDA')
endif

if emulator == 'PipeBot'
jaffarCPPFlags += '-D__JAFFAR_USE_PIPEBOT'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_PIPEBOT'
subdir('pipeBot')
endif

if emulator == 'TestEmulator'
jaffarCPPFlags += '-D__JAFFAR_USE_TESTEMULATOR'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_TESTGAME'
subdir('testEmulator')
endif

if emulator == 'ExciteBikeNative'
jaffarCPPFlags += '-D__JAFFAR_USE_EXCITEBIKENATIVE'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_NES'
subdir('exciteBikeNative')
endif

if emulator == 'NesHawk'
jaffarCPPFlags += '-D__JAFFAR_USE_NESHAWK'
jaffarCPPFlags += '-D__JAFFAR_ENABLE_NES'
subdir('nesHawk')
endif
