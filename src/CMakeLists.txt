add_subdirectory(data_readers)
add_subdirectory(layers)
add_subdirectory(models)
add_subdirectory(optimizers)
add_subdirectory(callbacks)
add_subdirectory(io)
add_subdirectory(utils)
add_subdirectory(regularization)

add_sources(lbann_Elemental_extensions.cpp 
            lbann_params.cpp
            lbann_comm.cpp
            )

