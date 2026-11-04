🎀 Heidi – Embedded AI Voice Assistant

An AI-powered voice assistant built using an ESP32 microcontroller, OLED display, Python backend and a locally running Large Language Model (LLM).

Heidi is designed to provide voice-based interaction with a cute animated face displayed on an OLED screen. It listens to user questions, processes speech, generates AI responses and speaks back to the user.

📌 Project Overview

Heidi combines embedded hardware with Artificial Intelligence to create an interactive voice assistant.

The ESP32 handles hardware interaction and OLED animations, while the Python backend manages speech processing and AI communication.

The project uses Ollama to run an AI model locally, reducing dependence on paid AI APIs.

✨ Features

- Voice-based interaction.
- AI-powered question answering.
- Local AI processing using Ollama.
- Speech-to-text conversion.
- Text-to-speech responses.
- Animated OLED face with different expressions.
- Python-based backend.
- Communication between hardware and software.
- Weather information support through an online weather service.

🛠️ Technologies Used

Technology Purpose
Python Backend development
FastAPI Backend API
Ollama Local AI model execution
Qwen 2.5:3B Language model
Faster-Whisper Speech recognition
pyttsx3 Text-to-speech
ESP32 WROOM Embedded hardware controller
OLED Display Animated facial expressions
Arduino IDE ESP32 firmware development

🔧 Hardware Components

- ESP32 WROOM development board
- OLED display
- Microphone module
- Speaker
- Audio amplifier
- Connecting wires
- Breadboard
- USB power supply

💻 Software Requirements

- Python 3.10 or later (use a compatible environment for the installed libraries).
- Ollama
- Arduino IDE
- Visual Studio Code
- Required Python libraries.

🚀 Installation and Setup

1. Clone the Repository

git clone https://github.com/YOUR_USERNAME/Heidi-Voice-Assistant.git

2. Navigate to the Project

cd Heidi-Voice-Assistant

3. Create a Virtual Environment

python -m venv venv

Activate it on Windows:

venv\Scripts\activate

4. Install Dependencies

pip install -r requirements.txt

5. Install Ollama

Download Ollama from:

https://ollama.com/

Pull the AI model:

ollama pull qwen2.5:3b

Start Ollama:

ollama serve

6. Run the Python Backend

uvicorn backend_app:app --reload

The FastAPI documentation can be accessed at:

http://127.0.0.1:8000/docs

7. Upload ESP32 Firmware

- Open the ESP32 firmware in Arduino IDE.
- Select the appropriate ESP32 board.
- Configure the required Wi-Fi and backend settings.
- Connect the ESP32 board.
- Upload the firmware.

⚙️ Working Principle

1. The user provides a voice command.
2. The microphone captures the audio.
3. The Python backend processes the recorded audio.
4. Faster-Whisper converts speech into text.
5. The text is sent to Ollama.
6. Qwen 2.5:3B generates an appropriate response.
7. pyttsx3 converts the response into speech.
8. The response is played through the speaker.
9. The OLED display shows the assistant’s animated expressions.

🧠 AI Model

Ollama – Qwen 2.5:3B

Ollama manages the local execution of the Qwen 2.5:3B language model. The model processes user questions and generates conversational responses.

The model runs on the computer hosting Ollama rather than directly on the ESP32.

📷 Prototype

Add photographs of your assembled Heidi prototype here.

🔮 Future Enhancements

- Wake-word activation.
- Improved conversational memory.
- More OLED facial expressions.
- Additional language support.
- Improved offline functionality.
- Smart-home integration.

🎯 Applications

- Educational demonstrations.
- Embedded AI learning.
- Voice-based human-computer interaction.
- IoT applications.
- AI and speech-processing experiments.

👩‍💻 Project Contributors

B. Greeshmanth

Shaik Afifa Anjum

Department of AI and Data Science

Vel Tech Rangarajan Dr. Sagunthala R&D Institute of Science and Technology, Chennai.

📄 License

This project is intended for educational and research purposes.

⸻

Heidi – Making Embedded AI More Interactive! 🎀
