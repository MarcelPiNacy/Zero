
Vector(type T):
{
	type R
	{
		T x;
		T y;
	}

	return R
}

FundamentalTypeTest():
{
	int x = 64;
	uint y = x as uint;
	float(64) z = 64.0;
	z /= 64.0;
}

ControlFlowTest():
{
	bool flag = true;

	if flag do
		flag = false;
	elif !flag do
		flag = true;
	else
		flag = true;

	if !flag
	{
		flag = true
	}
	elif !flag
	{
	}
	else
	{
	}

	while flag do
		flag = false;

	flag = true;

	while flag
	{
		flag = false
	}

	do
	{
		flag = true
	} while !flag;

	for let i = 0; i != 10; ++i do
		flag = false;
	
	for let i = 0; i != 10; ++i
	{
		flag = false;
	}

	for i: range do
		flag = false;

	for i: range
	{
		flag = false
	}

	select 10
	{
	if 0: {}
	if 1: {}
	if 2: {}
	if 3: {}
	if 10: {}
	else: {}
	}
}



main():
{
	return 0
}