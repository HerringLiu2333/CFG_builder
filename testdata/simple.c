// 本文件用于放置最小 C 测试样例，验证单 TU CFG 生成流程。

int sum_until_limit(int n) {
	int i = 0;
	int sum = 0;

	while (i < n) {
		if (i % 2 == 0) {
			sum += i;
		} else {
			sum -= 1;
		}

		if (sum > 20) {
			break;
		}

		i++;
	}

	return sum;
}

int main(void) {
	int result = sum_until_limit(10);
	if (result > 0) {
		return 0;
	}
	return 1;
}