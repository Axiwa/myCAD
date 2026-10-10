# Little CAD Viewer

基于 C++17 + OCCT 8.0.1 + OpenGL 3.3 实现的工业可视化项目。主要功能是 STEP 装配体的导入、三角化和绘制，未使用OCCT自带的可视化功能。使用 XCAF 进行模型的导入，导入过程中建立该文件的多叉树并储存prototype的mesh信息。使用 ImGUI 展示渲染信息和场景层级，支持点击选择高亮和控制显隐。使用 OrbitCamera 进行视角的交互，包括旋转、平移、缩放，将选中物体放到中心。绘制方面考虑了零件的镜像情况以及各零件在文件中自带的颜色。优化方面实现了 Frustum culling 和 Instancing，在测试模型高达65542个时，打开instancing绘制时间从24ms降到12ms。利用 fragment shader 做了一个简单的拔模角分析。

<video src="docs/myCAD.mp4" controls="controls" width="%80" height="%80"></video>