#Find glslc version compatiable with raytracing!
C:\VulkanSDK\1.3.239.0\Bin\glslc.exe shader.vert -o vert.spv
C:\VulkanSDK\1.3.239.0\Bin\glslc.exe shader.frag -o frag.spv
"C:\Users\donov\Desktop\Coding Area\Rendering Practice\glslang-main\OutputBuild\StandAlone\Release\glslangValidator.exe" -V --target-env vulkan1.3 -o rgen.spv raytrace.rgen
"C:\Users\donov\Desktop\Coding Area\Rendering Practice\glslang-main\OutputBuild\StandAlone\Release\glslangValidator.exe" -V --target-env vulkan1.3 -o rmiss.spv raytrace.rmiss
"C:\Users\donov\Desktop\Coding Area\Rendering Practice\glslang-main\OutputBuild\StandAlone\Release\glslangValidator.exe" -V --target-env vulkan1.3 -o rchit.spv raytrace.rchit
pause