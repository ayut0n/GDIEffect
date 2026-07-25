# GDIEffect
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat&logo=c%2B%2B) ![Build](https://img.shields.io/badge/Build-CMake-064F8C?style=flat&logo=cmake) ![Platform](https://img.shields.io/badge/Platform-Windows-0078D6?style=flat&logo=windows)

**[Русский](#русский)** | **[English](#english)**

---

<a id="русский"></a>
# Russian

Программа, вызывающая GDI (Graphics Device Interface) эффекты на вашем рабочем столе.

### Принцип работы

1. Программа создаёт **невидимый полноэкранный оверлей** поверх всех мониторов.
2. Раз в ~40 мс содержимое всех экранов копируется в буфер оверлея (`BitBlt` с `GetDC(NULL)`). Каждому пикселю принудительно выставляется альфа‑канал 255, чтобы оверлей был полностью непрозрачным.
3. Список из 15 эффектов переключается каждые ~1 секунду. Есть два типа эффектов:
- **Пиксельные** – напрямую изменяют массив байтов буфера (инверсия, таяние, волны, сортировка пикселей, пикселизация, туннель и т.д.).
- **Рисующие на HDC** – используют GDI-функции (линии, дублирование окон, перемешивание блоков, линза под курсором) на контексте оверлея.
4. Изменённый буфер выводится на экран через `UpdateLayeredWindow` с флагом `ULW_ALPHA` и `SourceConstantAlpha = 255`. Окно висит поверх всех окон, пропускает клики мыши (`WS_EX_TRANSPARENT`), но полностью перекрывает изображение рабочего стола своими искажениями.
5. В отдельном потоке генерируется 8‑битный bytebeat‑звук. Используются 6 математических формул, которые автоматически переключаются каждые 2.5–4.5 секунд. Звук выводится через `waveOutWrite` на максимальной громкости.
6. Ещё один поток каждые 5–25 мс принудительно перемещает курсор в случайную точку виртуального экрана, делая управление невозможным.
   
### Управление:

Ctrl+Shift+X - завершение работы программы

### Установка

1. Перейдите в раздел [Release](https://github.com/ayut0n/GDIEffect/releases/tag/v1.0.0) и скачайте файл GDIEffect.exe
2. Запустите GDIEffect.exe

### Самостоятельная сборка

Для самостоятельной сборки программы вам понадобится Visual Studio Code, а также расширение CMake Tools. Для компиляции программы установите MSYS2 MinGW.

1. Скачайте исходный код программы в ZIP-архиве.
2. Распакуйте архив
3. В терминале Visual Studio Code перейдите в место распаковки архива (папку для сборки)
4. Сгенерируйте файлы сборки и скомпилируйте в режиме Release:

```
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```
5. Готовый файл .exe появится в папке `build`
   
### ПРОГРАММА НЕ СОДЕРЖИТ ВИРУСОВ, СКРИПТОВ УНИЧТОЖЕНИЯ ДАННЫХ, НЕ ПОВРЕЖДАЕТ MBR И НЕ ВЫЗЫВАЕТ КРИТИЧЕСКИХ ОШИБОК СИСТЕМЫ.
### ЭТО ИСКЛЮЧИТЕЛЬНО ДЕМОНСТРАЦИЯ GDI‑ЭФФЕКТОВ И АУДИОГЕНЕРАЦИИ.

---

<a id="english"></a>
## English

A program that creates GDI (Graphics Device Interface) effects on your desktop.

### How It Works

1. The program creates an **invisible full-screen overlay** on top of all monitors.
2. Approximately every 40 ms, the contents of all screens are copied to the overlay buffer (`BitBlt` with `GetDC(NULL)`). Each pixel is forced to have an alpha channel value of 255 so that the overlay is completely opaque.
3. A list of 15 effects cycles through every ~1 second. There are two types of effects:
- **Pixel-based** – directly modify the buffer’s byte array (inversion, fade, waves, pixel sorting, pixelation, tunnel, etc.).
- **HDC-drawing** – use GDI functions (lines, window duplication, block blending, cursor lens) on the overlay context.
4. The modified buffer is displayed on the screen via `UpdateLayeredWindow` with the `ULW_ALPHA` flag and `SourceConstantAlpha = 255`. The window sits on top of all other windows, allows mouse clicks to pass through (`WS_EX_TRANSPARENT`), but completely overlays the desktop image with its distortions.
5. An 8-bit bytebeat sound is generated in a separate thread. Six mathematical formulas are used, which automatically switch every 2.5–4.5 seconds. The sound is output via `waveOutWrite` at maximum volume.
6. Another thread forcibly moves the cursor to a random point on the virtual screen every 5–25 ms, making it impossible to control the cursor.

### Controls:

Ctrl+Shift+X - Exit the program

### Installation

1. Go to the [“Release” section](https://github.com/ayut0n/GDIEffect/releases/tag/v1.0.0) and download the GDIEffect.exe file
2. Run GDIEffect.exe

### Building from Source

To build the program from source, you’ll need Visual Studio Code and the CMake Tools extension. To compile the program, install MSYS2 MinGW.

1. Download the program’s source code in a ZIP archive.
2. Extract the archive
3. In the Visual Studio Code terminal, navigate to the directory where the archive was extracted (build folder)
4. Generate the build files and compile in Release mode:
```
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```
5. The finished .exe file will appear in the `build` folder

### THE PROGRAM DOES NOT CONTAIN VIRUSES OR DATA-DESTRUCTION SCRIPTS, DOES NOT DAMAGE THE MBR, AND DOES NOT CAUSE CRITICAL SYSTEM ERRORS.
### THIS IS SOLELY A DEMONSTRATION OF GDI EFFECTS AND AUDIO GENERATION.
