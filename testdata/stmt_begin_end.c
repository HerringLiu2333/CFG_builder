int sink;

void f2fs_err(int sbi, const char* msg, int segno, int type, int sum_footer_entry_type) {
	(void)sbi;
	(void)msg;
	(void)segno;
	(void)type;
	sink = sum_footer_entry_type;
}

int demo_stmt_begin_end(int sbi, int segno, int type, int sum_footer_entry_type) {
	if (type != sum_footer_entry_type) {
		f2fs_err(sbi, "Inconsistent segment (%u) type [%d, %d] in SSA and SIT",
			 segno, type, sum_footer_entry_type);
	}
	return sink;
}
