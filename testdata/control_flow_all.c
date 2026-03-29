// 本文件用于覆盖 C 主要控制语句及简单组合，便于 CFG/IR 测试。

int all_control_flow(int n, int m) {
	int x = 0;
	int i = 0;
	int guard = 0;

	// if / else if / else
	if (n < 0) {
		x = -1;
	} else if (n == 0) {
		x = 10;
	} else {
		x = 1;
	}

	// for + continue + break
	for (i = 0; i < n; ++i) {
		if ((i % 2) == 0) {
			continue;
		}
		x += i;
		if (x > 100) {
			break;
		}
	}

	// while + goto + label + 空语句
	while (m > 0) {
		--m;
		if (m == 3) {
			goto adjust;
		}
		;
	}

adjust:
	x += 2;

	// do ... while
	do {
		++guard;
		x += guard;
	} while (guard < 2);

	// switch / case / default
	switch (x % 4) {
	case 0:
		x += 40;
		break;
	case 1:
		x += 11;
		break;
	case 2:
		x += 22;
		break;
	default:
		x -= 5;
		break;
	}

	return x;
}

int combo_loop_switch(int limit) {
	int acc = 0;
	int i = 0;

	// while + switch + if 组合
	while (i < limit) {
		switch (i % 3) {
		case 0:
			acc += i;
			break;
		case 1:
			if (acc > 10) {
				++i;
				continue;
			}
			acc -= 1;
			break;
		default:
			acc += 2;
			break;
		}
		++i;
	}

	return acc;
}

int combo_for_nested_if(int n) {
	int sum = 0;
	int i = 0;
	int j = 0;

	// 双层 for + 内层 if/else
	for (i = 0; i < n; ++i) {
		for (j = 0; j < n; ++j) {
			if (i == j) {
				sum += i;
			} else {
				sum += 1;
			}
		}
	}

	if (sum > 200) {
		return sum;
	}
	return sum + 1;
}

