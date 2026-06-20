ExplorerMaps
Developers:\n
-Brenes Ruiz Andy Antony
-Ortiz Vega Bianka Marcela
-Castillo Vasquez Jeferson Evener

Technologies:\n
- c++
- OpenGL
- GLEW
- GLFW
- GLM
- Dear Imgui
- Audio
- Miniaudio

Features:
- 3D project developed in C++ with OpenGL.
- Exploration of an urban environment with 3D models.
- Camera and player movement system.
- Controllable vehicle within the scene.
- Dynamic weather and environment system.
- Main menu and interface using ImGui.
- Dependencies included in the project to facilitate compilation.


Description:

- Explorer Maps is an interactive virtual museum,
primarily developed in C++ with OpenGL, 
that allows users to explore a 3D city environment
and recreates several tourist attractions, 
including two national and one South American site.
Users can explore the city in first - person view, drive a vehicle, 
change environmental conditions, and experience visual effects such as lighting,
weather, fog, shadows, and a dynamic sky.

- The tour combines 3D rendering, environmental interaction,
ambient audio, a traffic system, custom menus,
and tourist areas to create a immersive virtual exploration experience.

#Download the project:

To download this project to your computer you need to have Git installed.
This project already includes its main dependencies in the folder `dependencias/`, such as GLFW, GLEW, Assimp, GLM, ImGui and SOIL2.

1.Open a terminal or Git Bash.
2.Clone the repository with the following command:

```bash
1.git lfs install
2. git clone https://github.com/Andybrenes20/ExplorerMaps.git
3.cd ExplorerMan
4.git lfs pull
```
git lfs pull This is important because some 3D models, such as city.lb, they use Git LFS.

3. Open the project:

Open visual studio Then select Open a "project or solution" and you open the file "ExplorerMaps_V2.slnx".
To compile it, you must configure Visual Studio in Debug | x64 or Release | x64. It is recommended to use x64, since the project's 
library and dependency paths are configured for that platform.

Then you compile with Ctrl + Shift + B or go to Build > Build Solution, During compilation the project automatically 
copies the necessary resources to the output directory, such as: Models/Textures/Sounds/Image/DLLs from dependencies/bin/, etc.

#Screenshots:
- ![Uploading Captura de pantalla 2026-06-19 224458.png…]()
- ![Uploading Captura de pantalla 2026-06-19 224532.png…]()
- ![Uploading Captura de pantalla 2026-06-19 180556.png…]()
- ![Uploading Captura de pantalla 2026-06-19 224751.png…]()
- ![Uploading Captura de pantalla 2026-06-19 224821.png…]()

#DemoVideo:
- https://youtu.be/9D8KhX2-yto?si=vGA9mRmrJpG-iUnv


