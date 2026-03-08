idf_component_register(SRCS "main.cpp" "image_sender.cpp"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES Camera driver

)

target_add_binary_data(${COMPONENT_LIB} "image.jpg" BINARY)
