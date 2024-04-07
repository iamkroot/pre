add_library(LCM MODULE LazyCodeMotion.cpp)

# Get proper shared-library behavior (where symbols are not necessarily
# resolved when the shared library is linked) on OS X.
if(APPLE)
    set_target_properties(LCM PROPERTIES
        LINK_FLAGS "-undefined dynamic_lookup"
    )
endif(APPLE)