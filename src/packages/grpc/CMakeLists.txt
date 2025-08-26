# gRPC package is DISABLED
# This CMakeLists.txt file prevents the grpc package from being built
# while avoiding undefined reference errors

message(STATUS "gRPC package is disabled - no gRPC libraries available")

# Do not create any targets - this prevents the package from being built
# The absence of add_library() means no package_grpc target will be created
# and no grpc functions will be linked into the final executable