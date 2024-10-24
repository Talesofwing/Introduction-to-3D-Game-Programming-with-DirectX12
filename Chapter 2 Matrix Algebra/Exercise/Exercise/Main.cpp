#include <iostream>

using namespace std;

#define endl "\n"

const float EPSILON = 0.0001f;

void PrintTab(int tapCount) {
	for (int i = 0; i < tapCount; ++i) {
		cout << "\t";
	}
}

void PrintMatrix(float** matrix, int degree, bool lastLine = true, int startTabCount = 0) {
	if (matrix == nullptr) {
		return;
	}

	for (int i = 0; i < degree; ++i) {
		PrintTab(startTabCount);

		for (int j = 0; j < degree; ++j) {
			cout << matrix[i][j];
			if (j != degree - 1) {
				cout << "\t";
			}
		}

		if (i != degree - 1) {
			cout << endl;
		}
	}

	if (lastLine) {
		cout << endl;
	}
}

float** MatrixSet(int degree) {
	float** matrix = new float* [degree];
	for (int row = 0; row < degree; ++row) {
		matrix[row] = new float[degree];
		fill_n(matrix[row], degree, 0.0f);
	}

	return matrix;
}

float** MatrixSet(float* values, int degree) {
	float** matrix = new float* [degree];
	for (int row = 0; row < degree; ++row) {
		matrix[row] = new float[degree];
		for (int col = 0; col < degree; ++col) {
			matrix[row][col] = values[row * degree + col];
		}
	}

	return matrix;
}

float** CalculateTranspose(float** matrix, int degree) {
	float** result = MatrixSet(degree);
	for (int i = 0; i < degree; ++i) {
		for (int j = 0; j < degree; ++j) {
			result[j][i] = matrix[i][j];
		}
	}
	return result;
}

float** CalculateMinor(float** matirx, int degree, int rowIndex, int colIndex) {
	float** result = MatrixSet(degree - 1);
	for (int row = 0; row < degree - 1; ++row) {
		for (int col = 0; col < degree - 1; ++col) {
			int temp_row = row, temp_col = col;
			if (row >= rowIndex) {
				temp_row++;
			}
			if (col >= colIndex) {
				temp_col++;
			}
			result[row][col] = matirx[temp_row][temp_col];
		}
	}

	return result;
}

int* GetMaxZeroCountRow(float** matrix, int degree) {
	// result[0]: row index, result[1]: max count
	int* result = new int[2] {0};
	for (int row = 0; row < degree; ++row) {
		int temp = 0;
		for (int col = 0; col < degree; ++col) {
			if (matrix[row][col] == 0) {
				temp++;
			}
		}

		if (temp > result[1]) {
			result[0] = row;
			result[1] = temp;
		}
	}

	return result;
}

int* GetMaxZeroCountCol(float** matrix, int degree) {
// result[0]: col index, result[1]: max count
	int* result = new int[2] {0};
	for (int col = 0; col < degree; ++col) {
		int temp = 0;
		for (int row = 0; row < degree; ++row) {
			if (matrix[row][col] == 0) {
				temp++;
			}
		}

		if (temp > result[1]) {
			result[0] = col;
			result[1] = temp;
		}
	}

	return result;
}

float CalculateDeterminant(float** matrix, int degree, int depth = 0) {
	// [a b]
	// [c d]
	// formula: ad - bc
	if (degree == 2) {
		float det = matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0];
		return det;
	} else if (degree == 1) {
		return matrix[0][0];
	}

	int* rowInf = GetMaxZeroCountRow(matrix, degree);
	PrintTab(depth + 1);
	cout << "RowIndex: " << rowInf[0] << ", ZeroCount: " << rowInf[1] << endl;
	int* colInf = GetMaxZeroCountCol(matrix, degree);
	PrintTab(depth + 1);
	cout << "ColumnIndex: " << colInf[0] << ", ZeroCount: " << colInf[1] << endl;

	float result = 0;
	if (colInf[1] > rowInf[1]) {
		PrintTab(depth + 1);
		cout << "Expanding along a column" << endl << endl;
		int colIndex = colInf[0];
		for (int rowIndex = 0; rowIndex < degree; rowIndex++) {
			float element = matrix[rowIndex][colIndex];
			if (element == 0) {
				// continue;
			}

			PrintTab(depth + 1);
			cout << "[Minor " << rowIndex + 1 << " by (" << rowIndex << ", " << colIndex << ")], and the element is " << element << endl;
			float** minor = CalculateMinor(matrix, degree, rowIndex, colIndex);
			PrintMatrix(minor, degree - 1, depth == 0, 2);

			float sign = -1;
			if ((rowIndex + colIndex) % 2 == 0) {
				sign = 1;
			}

			cout << endl;
			float det = sign * element * CalculateDeterminant(minor, degree - 1, depth + 1);
			PrintTab(depth + 1);
			cout << "Determinant: " << det << endl << endl;
			result += det;
		}
	} else {
		PrintTab(depth + 1);
		cout << "Expanding along a row" << endl << endl;
		int rowIndex = rowInf[0];
		for (int colIndex = 0; colIndex < degree; colIndex++) {
			float element = matrix[rowIndex][colIndex];
			if (element == 0) {
				// continue;
			}

			PrintTab(depth + 1);
			cout << "[Minor " << colIndex + 1 << " by (" << rowIndex << ", " << colIndex << ")], and the element is " << element << endl;
			float** minor = CalculateMinor(matrix, degree, rowIndex, colIndex);
			PrintMatrix(minor, degree - 1, depth == 0, 2);

			float sign = -1;
			if ((rowIndex + colIndex) % 2 == 0) {
				sign = 1;
			}

			cout << endl;
			float det = sign * element * CalculateDeterminant(minor, degree - 1, depth + 1);
			PrintTab(depth + 1);
			cout << "Determinant: " << det << endl << endl;
			result += det;
		}
	}

	return result;
}

float** CalculateAdjoint(float** matrix, int degree) {
	float** result = MatrixSet(degree);
	for (int row = 0; row < degree; ++row) {
		for (int col = 0; col < degree; ++col) {
			float sign = -1;
			if ((row + col) % 2 == 0) {
				sign = 1;
			}

			PrintTab(1);
			cout << "[Minor by (" << row << ", " << col << ")], and the element is " << matrix[row][col] << endl;
			float** minor = CalculateMinor(matrix, degree, row, col);
			PrintMatrix(minor, degree - 1, true, 2);
			cout << endl;
			result[row][col] = sign * CalculateDeterminant(minor, degree - 1, 1);
			PrintTab(1);
			cout << "Value at (" << row << ", " << col << ") is " << result[row][col] << endl << endl;
		}
	}

	return CalculateTranspose(result, degree);
}

float** CalculateInverse(float** matrix, int degree) {
	float det = CalculateDeterminant(matrix, degree);
	cout << "Final Determinant: " << det << endl << endl;

	if (abs(det) - EPSILON <= 0) {
		cout << "Determinant is 0, can't calculate inverse." << endl;
		return nullptr;
	}

	float** result = CalculateAdjoint(matrix, degree);

	cout << "Adjoin Matrix: " << endl;
	PrintMatrix(result, degree, true);
	cout << endl;

	for (int row = 0; row < degree; ++row) {
		for (int col = 0; col < degree; ++col) {
			result[row][col] = result[row][col] / det;
		}
	}

	return result;
}

int main() {
	float* values = new float[] {
		1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 2.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 4.0f, 0.0f,
			1.0f, 2.0f, 3.0f, 1.0f
	};

	//float* values = new float[] {1, 2,
	//	3, 4};

	//float* values = new float[] {1, 2, 3,
	//	4, 5, 6,
	//	7, 8, 9};

	//float* values = new float[] {1, 4, 8, 7,
	//	6, 5, 7, 8,
	//	1, 5, 7, 5,
	//	3, 5, 4, 7};

	//float* values = new float[] {1, 4, 8, 9,
	//	0, 1, 5, 7,
	//	0, 2, 7, 8,
	//	3, 4, 8, 7};

	int degree = 4;

	float** matrix = MatrixSet(values, degree);
	cout << "[Matrix]" << endl;
	PrintMatrix(matrix, degree, true, 1);
	cout << endl;

	cout << "[Transpose Matrix]" << endl;
	float** transpose = CalculateTranspose(matrix, degree);
	PrintMatrix(transpose, degree, true, 1);
	cout << endl;

	cout << "[Minor Matrix by (0, 0)]" << endl;
	float** minor = CalculateMinor(matrix, degree, 0, 0);
	PrintMatrix(minor, degree - 1, true, 1);
	cout << endl;

	cout << "[Determinant]" << endl;
	float det = CalculateDeterminant(matrix, degree);
	cout << "Final Determinant: " << det << endl;
	cout << endl;

	cout << "[Adjoint Matrix]" << endl;
	float** adjoint = CalculateAdjoint(matrix, degree);
	cout << "Final Matrix: " << endl;
	PrintMatrix(adjoint, degree);
	cout << endl;

	cout << "[Inverse Matrix]" << endl;
	float** inverse = CalculateInverse(matrix, degree);
	cout << "Final Matrix: " << endl;
	PrintMatrix(inverse, degree);
	cout << endl;
}
