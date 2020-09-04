#include <iostream>

using namespace std;

void PrintMatrix (float matrix[4][4]) {
	for (int i = 0; i < 4; ++i) {
		cout << matrix[i][0] << "\t";
		cout << matrix[i][1] << "\t";
		cout << matrix[i][2] << "\t";
		cout << matrix[i][3];
		cout << endl;
	}
}

void CalculateTranspose (float matrix[4][4]) {
	float result[4][4];
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			result[j][i] = matrix[i][j];
		}
	}
	PrintMatrix (result);
}

float CalculateDetrminant (float matrix[4][4]) {
	float result = 0.0f;
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			
		}
	}

	return result;
}

void CalculateInverse (float matrix[4][4]) {
	float det = CalculateDetrminant (matrix);

	float result[4][4];
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			result[j][i] = matrix[i][j];
		}
	}
	PrintMatrix (result);
}

void whatthefuck () {
	float matrix[4][4] = {{1.0f, 0.0f, 0.0f, 0.0f},
						   {0.0f, 2.0f, 0.0f, 0.0f},
						   {0.0f, 0.0f, 4.0f, 0.0f},
						   {1.0f, 2.0f, 3.0f, 1.0f}};

	PrintMatrix (matrix);

	cout << endl;

	CalculateTranspose (matrix);
}

int main () {
	whatthefuck ();
}