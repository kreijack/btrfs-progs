

setup() {
	# place here your setup including other
	# library

	# return a value different from zero 
	# to raise a concern. If the returned 
	# value is different from 0, teardown() is 
	# NOT called
	return 0
}

teardown() {
	# place here your cleanup
}

test_example() {
	# do your test
	
	# return 0 if the test is successful
	# return != 0 if fails
	return 0
}
