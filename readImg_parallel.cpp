#include <iostream>
#include <unistd.h>
#include <fstream>
#include <chrono>
#include <pthread.h>


#define FILE_OUT "parallel.bmp"

using std::cout;
using std::endl;
using std::ifstream;
using std::ofstream;

#pragma pack(1)
#pragma once
#define NUMBER_OF_THREADS 4

typedef int LONG;
typedef unsigned short WORD;
typedef unsigned int DWORD;

typedef struct tagBITMAPFILEHEADER {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER, *PBITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER {
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
} BITMAPINFOHEADER, *PBITMAPINFOHEADER;

struct PixelColor {
    unsigned char red;
    unsigned char green;
    unsigned char blue;
};

int rows;
int cols;
PixelColor **picture_input;
PixelColor **output;
pthread_mutex_t mutex_;
pthread_mutex_t mutex_most_popular;

bool fillAndAllocate(char *&buffer, const char *fileName, int &rows, int &cols, int &bufferSize) {
  std::ifstream file(fileName);

  if (file) {
    file.seekg(0, std::ios::end);
    std::streampos length = file.tellg();
    file.seekg(0, std::ios::beg);

    buffer = new char[length];
    file.read(&buffer[0], length);

    PBITMAPFILEHEADER file_header;
    PBITMAPINFOHEADER info_header;

    file_header = (PBITMAPFILEHEADER) (&buffer[0]);
    info_header = (PBITMAPINFOHEADER) (&buffer[0] + sizeof(BITMAPFILEHEADER));
    rows = info_header->biHeight;
    cols = info_header->biWidth;
    bufferSize = file_header->bfSize;
    return 1;
  } else {
    cout << "File" << fileName << " doesn't exist!" << endl;
    return 0;
  }
}

void getPixlesFromBMP24(int end, int rows, int cols, char *fileReadBuffer, PixelColor **&pixels) {
  int count = 1;
  int extra = cols % 4;
  pixels = new PixelColor *[rows];
  for (int i = 0; i < cols; i++) {
    pixels[i] = new PixelColor[cols];
  }
  for (int i = 0; i < rows; i++) {
    count += extra;
    for (int j = cols-1; j >=0; j--)
      for (int k = 0; k < 3; k++) {
        switch (k) {
          case 0:
            pixels[i][j].red = fileReadBuffer[end - count++];
            break;
          case 1:
            pixels[i][j].green = fileReadBuffer[end - count++];
            break;
          case 2:
            pixels[i][j].blue = fileReadBuffer[end - count++];
            break;
        }
        
      }
  }
}

void writeOutBmp24(char *fileBuffer, const char *nameOfFileToCreate, int bufferSize, PixelColor **input) {
  std::ofstream write(nameOfFileToCreate);
  if (!write) {
    cout << "Failed to write " << nameOfFileToCreate << endl;
    return;
  }
  int count = 1;
  int extra = cols % 4;
  for (int i = 0; i < rows; i++) {
    count += extra;
    for (int j = cols-1; j >=0; j--)
      for (int k = 0; k < 3; k++) {
        switch (k) {
          case 0:
            fileBuffer[bufferSize - count++] = input[i][j].red;
            break;
          case 1:
            fileBuffer[bufferSize - count++] = input[i][j].green;
            break;
          case 2:
            fileBuffer[bufferSize - count++] = input[i][j].blue;
            break;
        }
        
      }
  }
  write.write(fileBuffer, bufferSize);
}

PixelColor get_mean(PixelColor **&input, int x, int y) {
  PixelColor mean;
  int green_sum = 0, red_sum = 0, blue_sum = 0, count = 0;
  for (int i = -1; i <= 1; ++i) {
    for (int j = -1; j <= 1; ++j) {
      if (x + 1 < 0 || x + i >= rows)
        continue;
      if (y + 1 < 0 || y + j >= cols)
        continue;
      green_sum += input[x + i][y + j].green;
      red_sum += input[x + i][y + j].red;
      blue_sum += input[x + i][y + j].blue;
      count++;
    }
  }
  green_sum /= count;
  red_sum /= count;
  blue_sum /= count;
  mean.green = (unsigned char) green_sum;
  mean.red = (unsigned char) red_sum;
  mean.blue = (unsigned char) blue_sum;
  return mean;
}

PixelColor get_mean_all(PixelColor **&input) {
  PixelColor mean;
  int green_sum = 0, red_sum = 0, blue_sum = 0, count = 0;
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      green_sum += (int) input[i][j].green;
      red_sum += (int) input[i][j].red;
      blue_sum += (int) input[i][j].blue;
    }
  }
  count = rows * cols;
  green_sum /= count;
  red_sum /= count;
  blue_sum /= count;
  mean.green = green_sum;
  mean.red = red_sum;
  mean.blue = blue_sum;
  return mean;
}

void delete_pixels(PixelColor **&input) {
  for (int i = 0; i < cols; i++) {
    delete[] input[i];
  }
  delete[] input;
}

void* filter_horizontal(void* tid) {
  long num = (long)tid;
  
  
  for (int i = num*((int) rows/NUMBER_OF_THREADS); i < (num+1)*((int) rows/NUMBER_OF_THREADS); i++) {
  
    for (int j = 0; j < cols; j++) {
      
      int median=cols/2;
      int dist=j-median;
    
      output[i][j]=picture_input[i][median-dist];
      
    }
  }
//  delete_pixels(input);
  picture_input = output;
}

void *filter_vertical(void* tid) {
  long num = (long)tid;
  
  for (int i = num*((int) rows/NUMBER_OF_THREADS); i < (num+1)*((int) rows/NUMBER_OF_THREADS); i++) {
    for (int j = 0; j < cols; ++j) {
      output[i][j] = picture_input[rows-i-1][j];
    }
  }
 // delete_pixels(input);
  picture_input = output;
}

void* Sepia(void* tid){
  long num = (long)tid;

  for (int i = num*((int) rows/NUMBER_OF_THREADS); i < (num+1)*((int) rows/NUMBER_OF_THREADS); i++) {
    for (int j = 0; j < cols; j++) {
    // for (int j = num*((int) cols/NUMBER_OF_THREADS); j < (num+1)*((int) cols/NUMBER_OF_THREADS); j++) {

      int red_color = picture_input[i][j].red*(0.393)+picture_input[i][j].blue*(0.189)+picture_input[i][j].green*(0.769);
      int blue_color = picture_input[i][j].blue*(0.131)+picture_input[i][j].red*(0.272)+picture_input[i][j].green*(0.534);
      int green_color = picture_input[i][j].blue*(0.168)+picture_input[i][j].red*(0.349)+picture_input[i][j].green*(0.686);
       if (red_color >= UINT8_MAX) output[i][j].red = UINT8_MAX;
            else output[i][j].red = red_color;

            if (green_color >= UINT8_MAX) output[i][j].green = UINT8_MAX;
            else output[i][j].green = green_color;

            if (blue_color >= UINT8_MAX) output[i][j].blue = UINT8_MAX;
            else output[i][j].blue = blue_color;

    }
  }
  // pthread_mutex_unlock (&mutex_);
  // reviews_csv.close();
  pthread_exit(NULL);
 // delete_pixels(input);
  picture_input = output;
}

void* sharpen(void* tid) {
  long num = (long)tid;

  int kernel[3][3] = {{ 0, -1, 0}, { -1, 5, -1}, {0, -1, 0}};
  int sum = 0;
  for (int i = num*((int) rows/NUMBER_OF_THREADS)+1; i < (num+1)*((int) rows/NUMBER_OF_THREADS) - 1; i++) // 1 - rows-1
    for (int j = 1; j < cols - 1; j++)
    for(int k = 0 ; k < 3 ; k++)
    {
        sum = 0;
        for (int m = -1; m <= 1; m++)
          for (int n = -1; n <= 1; n++){

            if(k==0){
              sum += (kernel[m + 1][n + 1] * picture_input[i + m][j + n].red) ;
            }
            else if(k==1){
              sum += (kernel[m + 1][n + 1] * picture_input[i + m][j + n].blue);
            }
            else{ sum += (kernel[m + 1][n + 1] * picture_input[i + m][j + n].green);
            }
            
          }
        
        if (sum < 0)
          sum = 0;
        if (sum > UINT8_MAX)
          sum = UINT8_MAX;
        if(k == 0)
          output[i][j].red=sum;
        else if (k==1)
          output[i][j].green=sum;
        else 
          output[i][j].blue=sum  ;
    }
  
  //delete_pixels(input);
  picture_input = output;

        
      

}

void* add_X(void* tid){
  long num = (long)tid;

  for(int i = num*((int) rows/NUMBER_OF_THREADS); i < (num+1)*((int) rows/NUMBER_OF_THREADS); i++) {
    for(int j = 0; j < cols; j++) {
      output[i][j] = picture_input[i][j];
    }
  }

  for (int i = num*((int) rows/NUMBER_OF_THREADS); i < (num+1)*((int) rows/NUMBER_OF_THREADS); i++) {
    int j = i;
    int k = cols - 1 - i;
      if (j < cols) {
        //cout << "HERE: " << j << endl;
        output[i][j].red = 0;
        output[i][j].blue = 0;
        output[i][j].green = 0;

        output[i][k].red = 0;
        output[i][k].blue = 0;
        output[i][k].green = 0;
      }
  }

  pthread_exit(NULL);
  //delete_pixels(input);
  picture_input = output;

}


int main(int argc, char *argv[]) {
  auto start_time = std::chrono::high_resolution_clock::now();
  char *fileBuffer;
  int bufferSize;
  char *fileName = argv[1];
  
  if (!fillAndAllocate(fileBuffer, fileName, rows, cols, bufferSize)) {
    cout << "File read error" << endl;
    return 1;
  }

  output = new PixelColor *[rows];
  for (int i = 0; i < rows; i++) {
    output[i] = new PixelColor[cols];
  }

  getPixlesFromBMP24(bufferSize, rows, cols, fileBuffer, picture_input);
  void *status;

  pthread_t threads[NUMBER_OF_THREADS];

  // add_X ----------------
  // for(long tid = 0; tid < NUMBER_OF_THREADS; tid++) {
  //   pthread_create(&threads[tid], NULL, add_X, (void*)tid);
	// }
  // for(long i = 0; i < NUMBER_OF_THREADS; i++)
  //   pthread_join(threads[i], &status);

  
  // Sepia -----------------
  // for(long tid = 0; tid < NUMBER_OF_THREADS; tid++) {
	// 	pthread_create(&threads[tid], NULL, Sepia, (void*)tid);
  
	// }
  // for(long i = 0; i < NUMBER_OF_THREADS; i++)
  //   pthread_join(threads[i], &status);

  // Sharpen ----------------
  // for(long tid = 0; tid < NUMBER_OF_THREADS; tid++) {
		
  //   pthread_create(&threads[tid], NULL, sharpen, (void*)tid);
	// }
  // for(long i = 0; i < NUMBER_OF_THREADS; i++)
  //   pthread_join(threads[i], &status);
    // vertical
  for(long tid = 0; tid < NUMBER_OF_THREADS; tid++) {
		
    pthread_create(&threads[tid], NULL, filter_vertical, (void*)tid);
	}
  for(long i = 0; i < NUMBER_OF_THREADS; i++)
    pthread_join(threads[i], &status);
    //////horizantal
  // for(long tid = 0; tid < NUMBER_OF_THREADS; tid++) {
		
  //   pthread_create(&threads[tid], NULL, filter_horizontal, (void*)tid);
	// }
  // for(long i = 0; i < NUMBER_OF_THREADS; i++)
  //   pthread_join(threads[i], &status);


  

  writeOutBmp24(fileBuffer, FILE_OUT, bufferSize, output);
  auto end_time = std::chrono::high_resolution_clock::now();
  auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
  cout << "Execution Time: " << ms_int << endl;
  // cout << cols << endl;
  // cout << rows << endl;
  return 0;
}