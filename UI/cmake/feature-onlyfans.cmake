if(TARGET OBS::browser-panels)
  target_sources(obs-studio PRIVATE auth-oauth-onlyfans.cpp auth-oauth-onlyfans.hpp auth-onlyfans.cpp auth-onlyfans.hpp)
  target_enable_feature(obs-studio "Onlyfans API connection" ONLYFANS_ENABLED)
else()
  target_disable_feature(obs-studio "Onlyfans API connection")
endif()
