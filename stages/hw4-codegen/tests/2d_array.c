void main() {
    int arr[2][3]; 
    int i, j;

	for (i = 0; i < 2; i = i + 1) {
		for (j = 0; j < 3; j = j + 1) {
			arr[i][j] = i;
		}
	}

	for (j = 0; j < 3; j = j + 1) {
		for (i = 0; i < 2; i = i + 1) {
			write(arr[i][j]);
		}
	}

}