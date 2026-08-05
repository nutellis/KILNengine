add_subdirectory(algorithms)
add_subdirectory(concepts)
add_subdirectory(containers)
add_subdirectory(memory)
add_subdirectory(type_traits)

target_sources(${PROJECT_NAME} PRIVATE
        contracts.cxx
)

target_sources(${PROJECT_NAME} PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}/src
        FILES
        contract_macros.hpp
)

target_sources(${PROJECT_NAME} PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${PROJECT_SOURCE_DIR}/src
        FILES
        util.cppm
        any_cast.ixx
        Bool.ixx
        contracts.ixx
        Dummy.ixx
        EnumMask.ixx
        FunctionWrapper.ixx
        Lazy.ixx
        Overloaded.ixx
        reflection.ixx
        reinterpret_any_cast.ixx
        ScopeFail.ixx
        StringLiteral.ixx
        Strong.ixx
        TypeList.ixx
)
