# MainECU

Проект в Visual Studio Code + PlatformIO

MainECU.cpp - Рабочий файл проекта + эмулятор

Для сборки необходимо создать файл `platformio_local.ini` со следующим содержанием:
```
[env]
upload_port = COM3
monitor_port = COM3
```
Где необходимо указать номер порта для загрузки и консоли.
___

Ниже файлы только для отладки, не использовать!\
MainECU_L3_Emulator - Эмулятор блока MainECU ( отвечает на запросы ).\
MainECU_L3_Android - Эмулятор планшета ( отправляем запросы и получает ответы ).\

