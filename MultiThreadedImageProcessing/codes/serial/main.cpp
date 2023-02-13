#include <iostream>
#include <unistd.h>
#include <fstream>
#include <vector>
#include <ctime>
#include <pthread.h>
#include <thread>

using namespace std;

#pragma pack(1)

typedef int LONG;
typedef unsigned short WORD;
typedef unsigned int DWORD;

typedef struct TagBitMapFileHeader
{
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BitMapFileHeader, *P_BitMapFileHeader;


typedef struct TagBitMapInfoHeader
{
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BitMapInfoHeader, *P_BitMapInfoHeader;

typedef struct Pixel{
    int red;
    int green;
    int blue;
    Pixel(int r, int g, int b){
        red = max(0, min(255,r));
        green = max(0, min(255,g));
        blue = max(0, min(255,b));
    }
    Pixel(){
        red = 0;
        green = 0;
        blue = 0;
    }
} Pixel;

int Kernel[3][3] = {
        {-2, -1, 0},
        {-1, 1, 1},
        {0, 1, 2}
};

bool fillAndAllocate(char *&buffer, const char *fileName, int &rows, int &cols, int &bufferSize) {
    std::ifstream file(fileName);

    if (!file) {
        cout << "File" << fileName << " doesn't exist!" << endl;
        return false;
    }
    file.seekg(0, std::ios::end);
    std::streampos length = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer = new char[length];
    file.read(&buffer[0], length);

    P_BitMapFileHeader file_header;
    P_BitMapInfoHeader info_header;

    file_header = (P_BitMapFileHeader) (&buffer[0]);
    info_header = (P_BitMapInfoHeader) (&buffer[0] + sizeof(BitMapFileHeader));
    rows = info_header->biHeight;
    cols = info_header->biWidth;
    bufferSize = file_header->bfSize;
    return true;
}

inline Pixel get_pixel(const char* buffer, int red_index){
    return {
        int((unsigned char)buffer[red_index]),
        int((unsigned char)buffer[red_index-1]),
        int((unsigned char)buffer[red_index-2]),
    };
}

vector<vector<Pixel>> getPixelsFromBMP24(int end, int& rows, int& cols, const char *fileReadBuffer) {
    int count = 1;
    int extra = cols % 4;
    vector<vector<Pixel>> image(rows, vector<Pixel>(cols));
    for (int i = 0; i < rows; i++) {
        count += extra;
        for (int j = cols - 1; j >= 0; j--) {
            for (int k = 0; k < 3; k++)
            {
                unsigned char channel = fileReadBuffer[end-count];
                count++;
                switch (k)
                {
                    case 0:
                        // fileReadBuffer[end - count] is the red value
                        image[i][j].red = int(channel);
                        break;
                    case 1:
                        // fileReadBuffer[end - count] is the green value
                        image[i][j].green = int(channel);
                        break;
                    case 2:
                        // fileReadBuffer[end - count] is the blue value
                        image[i][j].blue = int(channel);
                        break;
                        // go to the next position in the buffer
                }
            }
//            image[i][j] = get_pixel(fileReadBuffer, end - count);
//            count += 3;
        }
    }
    return image;
}

void writeOutBmp24(char *fileBuffer, const char *nameOfFileToCreate, int bufferSize, int& rows, int& cols, vector<vector<Pixel>>& image) {
    std::ofstream write(nameOfFileToCreate);
    if (!write) {
        cout << "Failed to write " << nameOfFileToCreate << endl;
        return;
    }
    int count = 0;
    int extra = cols % 4;
    for (int i = 0; i < rows; i++) {
        count += extra;
        for (int j = cols - 1; j >= 0; j--) {
            for (int k = 0; k < 3; k++) {
                count++;
                switch (k) {
                    case 0:
                        // write red value in fileBuffer[bufferSize - count]
                        fileBuffer[bufferSize - count] = char(image[i][j].red);
                        break;
                    case 1:
                        // write green value in fileBuffer[bufferSize - count]
                        fileBuffer[bufferSize - count] = char(image[i][j].green);
                        break;
                    case 2:
                        // write blue value in fileBuffer[bufferSize - count]
                        fileBuffer[bufferSize - count] = char(image[i][j].blue);
                        break;
                }
                // go to the next position in the buffer
            }
        }
    }
    write.write(fileBuffer, bufferSize);
}

void horizontal_mirror(vector<vector<Pixel>>& image, int& rows, int& cols)
{
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols / 2; j++)
        {
            Pixel temp = image[i][j];
            image[i][j] = image[i][cols - j - 1];
            image[i][cols - j - 1] = temp;
        }
    }
}

Pixel kernel_convolution(vector<vector<Pixel>>& image, int row, int col){
    int r=0, g=0, b=0;
    for(int i=-1; i<=1; i++){
        for(int j=-1; j<=1; j++){
            r += image[row+i][col+j].red * Kernel[i+1][j+1];
            g += image[row+i][col+j].green * Kernel[i+1][j+1];
            b += image[row+i][col+j].blue * Kernel[i+1][j+1];
        }
    }
    return {r, g, b};
}

void convolute_with_kernel(vector<vector<Pixel>>& image, int& rows, int& cols)
{
    vector<vector<Pixel>> temp(rows, vector<Pixel>(cols));
    for(int t=0; t<rows; t++) {
        temp[t][0] = image[t][0];
        temp[t][cols-1] = image[t][cols-1];
    }
    for(int t=0; t<cols; t++) {
        temp[0][t] = image[0][t];
        temp[rows-1][t] = image[rows-1][t];
    }

    for (int i = 1; i < rows - 1; i++)
        for (int j = 1; j < cols - 1; j++)
            temp[i][j] = kernel_convolution(image, i, j);
    image = temp;
}

void rhombus_edge(vector<vector<Pixel>>& image, int& rows, int& cols) {
    int half_row = rows / 2;
    for (int r = 0; r < rows; r++) {
        int columns[4] = {half_row - r, 3 * half_row - r, r - half_row, r + half_row};
        for (int j = 0; j < 4; j++) {
            if (columns[j] < 0 || columns[j]>=cols)
                continue;
            image[r][columns[j]].red = 255;
            image[r][columns[j]].green = 255;
            image[r][columns[j]].blue = 255;
        }
    }
}

int main(int argc, char *argv[])
{
    char *fileBuffer;
    int bufferSize;
    int rows, cols;
    if (argc < 2)
    {
        cout << "Please provide an input file name" << endl;
        return 0;
    }
    char *fileName = argv[1];
    auto start_time = std::chrono::high_resolution_clock::now();
    if (!fillAndAllocate(fileBuffer, fileName, rows, cols, bufferSize))
    {
        cout << "File read error" << endl;
        return 1;
    }

    // read input file
    auto image = getPixelsFromBMP24(bufferSize, rows, cols, fileBuffer);

    // apply filters
    horizontal_mirror(image, rows, cols);
    convolute_with_kernel(image, rows, cols);
    rhombus_edge(image, rows, cols);

    // write output file
    writeOutBmp24(fileBuffer, "output.bmp", bufferSize, rows, cols, image);

    auto end_time = std::chrono::high_resolution_clock::now();
    cout << "Execution Time: " << std::chrono::duration_cast<std::chrono::microseconds>(end_time-start_time).count() << endl;
    
    return 0;
}
