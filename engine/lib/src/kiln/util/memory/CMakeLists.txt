target_sources(${PROJECT_NAME} PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/src
        FILES
        lifetime_capture_by.hpp
        lifetimebound.hpp
        no_unique_address.hpp
)

target_sources(${PROJECT_NAME} PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${PROJECT_SOURCE_DIR}/src
        FILES
        memory.cppm
        construct_at_using_allocator.ixx
        Deleter.ixx
        LifeCycleEraseMechanism.ixx
        uses_allocator_construction_args.ixx
)
