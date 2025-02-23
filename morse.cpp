#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <stdexcept>
#include <cmath>
#include <cstdint>
#include <sstream>

const int SAMPLE_RATE = 8000;
const int TI_DURATION = SAMPLE_RATE / 5; // durée d'un ti

std::map<char, std::string> morseTable = {
    {'A', ".-"}, {'B', "-..."}, {'C', "-.-."}, {'D', "-.."},
    {'E', "."}, {'F', "..-."}, {'G', "--."}, {'H', "...."},
    {'I', ".."}, {'J', ".---"}, {'K', "-.-"}, {'L', ".-.."},
    {'M', "--"}, {'N', "-."}, {'O', "---"}, {'P', ".--."},
    {'Q', "--.-"}, {'R', ".-."}, {'S', "..."}, {'T', "-"},
    {'U', "..-"}, {'V', "...-"}, {'W', ".--"}, {'X', "-..-"},
    {'Y', "-.--"}, {'Z', "--.."},
    {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"},
    {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."},
    {'9', "----."}, {'0', "-----"},
    {' ', "|"}
};
std::map<std::string, char> reverseMorseTable;

class MorseEncoder {
public:
    static void encodeToWav(const std::string &message, const std::string &outputFile) {
        std::ofstream file(outputFile, std::ios::binary);
        std::vector<int16_t> audioData;
        int frequency = TI_DURATION; // comme ça un ti = une période
        int duration;

        for (char c : message) {
            std::string code = morseTable[toupper(c)];
            for (char symbol : code) {
                if (symbol == '.'){
                    duration = TI_DURATION;
                }else if (symbol == '-'){
                    duration = 3*TI_DURATION;
                }else if (symbol == '|'){ // espace
                    duration = 0;
                    for (int i = 0; i < 2*TI_DURATION; ++i) { // 3+2+3 = 7 ti entre deux mots
                        audioData.push_back(0);
                    }
                }
                for (int i = 0; i < duration; ++i) {
                    audioData.push_back(32767 * std::sin(2 * M_PI * frequency * i / SAMPLE_RATE));
                }
                // pause entre symboles
                for (int i = 0; i < TI_DURATION; ++i) {
                    audioData.push_back(0);
                }
            }
            // pause entre caractères
            for (int i = 0; i < 3*TI_DURATION; ++i) {
                audioData.push_back(0);
            }
        }
        writeWavHeader(file, audioData.size() * 2); // chaque entier (int16_t) étant codé sur 2 bit 
        file.write(reinterpret_cast<const char*>(audioData.data()), audioData.size() * 2);
        file.close();
    }

// boîte noire pour écrire l'entête d'un fichier .wav
private:
    static void writeWavHeader(std::ofstream &file, int dataSize) {
        file.write("RIFF", 4);
        int fileSize = 36 + dataSize;
        file.write(reinterpret_cast<const char*>(&fileSize), 4);
        file.write("WAVEfmt ", 8);
        int fmtChunkSize = 16;
        file.write(reinterpret_cast<const char*>(&fmtChunkSize), 4);
        int16_t audioFormat = 1;
        file.write(reinterpret_cast<const char*>(&audioFormat), 2);
        int16_t numChannels = 1;
        file.write(reinterpret_cast<const char*>(&numChannels), 2);
        file.write(reinterpret_cast<const char*>(&SAMPLE_RATE), 4);
        int byteRate = SAMPLE_RATE * 2;
        file.write(reinterpret_cast<const char*>(&byteRate), 4);
        int16_t blockAlign = 2;
        file.write(reinterpret_cast<const char*>(&blockAlign), 2);
        int16_t bitsPerSample = 16;
        file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
        file.write("data", 4);
        file.write(reinterpret_cast<const char*>(&dataSize), 4);
    }
};

class MorseDecoder {
public:
    static std::string decodeFromWav(const std::string &inputFile){
        // lecture audio
        std::ifstream file(inputFile, std::ios::binary);
        file.seekg(44);
        std::vector<int16_t> audioData;
        int16_t sample;
        while (file.read(reinterpret_cast<char*>(&sample), sizeof(sample))) {
            audioData.push_back(sample);
        }
        file.close();

        // afficher audioData pour débugger
        // std::ostringstream oss;
        // std::copy(audioData.begin(), audioData.end()-1,std::ostream_iterator<int>(oss, ","));
        // std::cout << oss.str()  << std::endl;

        // transciption audio en morse
        std::string morseCode;
        int pauseLength = 0;
        int soundLength = 0;
        for (size_t i = 1; i<audioData.size(); ++i){
            if (std::abs(audioData[i]) > 1000){ // si il y a du son
                soundLength = 0; // compteur incrémental de la longueur du signal => point ou trait
                while (i < audioData.size() && std::abs(audioData[i])+std::abs(audioData[i-1]) > 1000) { // le i-1 permet d'éviter de croire à une pause quand le signal passe par une valeur inférieure à 1000
                    soundLength++;
                    i++;
                }
                if (soundLength>2*TI_DURATION){ // on prend un peu moins de la durée d'un trait pour éviter une erreur.
                    morseCode += '-';
                }else{
                    morseCode += '.';
                }
            }else{
                pauseLength = 0; // compteur incrémental de la longueur de la pause => espaces entre caractères ou mots
                while (i < audioData.size() && std::abs(audioData[i])+std::abs(audioData[i-1]) < 1000) { // le i-1 permet d'éviter de croire à une pause quand le signal passe par une valeur inférieure à 1000
                    pauseLength++;
                    i++;
                }
                if (pauseLength > 5*TI_DURATION){
                    morseCode += '|';
                } else if (pauseLength > 2*TI_DURATION){
                    morseCode += ' ';
                }
            }
        }
        // transformation morse écrit en texte
        std::string decodedMessage;
        std::string currentSymbol;
        for (char c : morseCode) {
            if ((c == ' ') || (c=='|')) {
                if (!currentSymbol.empty()){
                    try{
                    decodedMessage += reverseMorseTable[currentSymbol];
                    }catch (const std::exception &e){
                        std::cerr<<"Erreur: "<<e.what()<<std::endl;
                    }
                    currentSymbol = "";
                    if (c=='|') {
                        decodedMessage += " ";
                    }
                }
            }else{
                currentSymbol += c;
            }
        }
        if (!currentSymbol.empty()){
            try{
                decodedMessage += reverseMorseTable[currentSymbol];
            }catch (const std::exception &e){
                std::cerr<<"Erreur: "<<e.what()<<std::endl;
            }
        }
        return decodedMessage;
    }
};

int main(int argc, char *argv[]){
    if (argc != 4){
        std::cerr << "Usage: " << argv[0] << " --encode mymessage.txt mymessage.wav" << std::endl;
        std::cerr << "       " << argv[0] << " --decode mymessage.wav mymessage.txt" << std::endl;
        return 1;
    }

    std::string mode = argv[1];
    std::string inputFile = argv[2];
    std::string outputFile = argv[3];
    
    for (const auto &pair : morseTable){
        reverseMorseTable[pair.second] = pair.first;
    }
    
    try{
        if (mode == "--encode"){
            std::ifstream file(inputFile);
            std::string message((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()); // quelle galère ce langage
            MorseEncoder::encodeToWav(message, outputFile);
        }else if (mode == "--decode"){
            std::ofstream outFile(outputFile);
            outFile << MorseDecoder::decodeFromWav(inputFile);
            outFile.close();
        }
    }catch(const std::exception &e){
        std::cerr<<"Erreur: "<<e.what()<<std::endl;
        return 1;
    }
    return 0;
}