#include "memory.hpp"

// Credits: learn_more, stevemk14ebr
size_t findPattern(const PBYTE rangeStart, size_t len, const char *pattern)
{
  size_t l = strlen(pattern);
  PBYTE patt_base = static_cast<PBYTE>(malloc(l >> 1));
  PBYTE msk_base = static_cast<PBYTE>(malloc(l >> 1));
  PBYTE pat = patt_base;
  PBYTE msk = msk_base;
  if (pat && msk)
  {
	l = 0;
	while (*pattern)
	{
	  if (*pattern == ' ')
		pattern++;
	  if (!*pattern)
		break;
	  if (*(PBYTE)pattern == (BYTE)'\?')
	  {
		*pat++ = 0;
		*msk++ = '?';
		pattern += ((*(PWORD)pattern == (WORD)'\?\?') ? 2 : 1);
	  }
	  else
	  {
		*pat++ = getByte(pattern);
		*msk++ = 'x';
		pattern += 2;
	  }
	  l++;
	}
	*msk = 0;
	pat = patt_base;
	msk = msk_base;
	for (size_t n = 0; n < (len - l); ++n)
	{
	  if (isMatch(rangeStart + n, patt_base, msk_base))
	  {
		free(patt_base);
		free(msk_base);
		return n;
	  }
	}
	free(patt_base);
	free(msk_base);
  }
  return -1;
}

uint64_t Memory::get_proc_baseaddr() { return baseaddr; }

process_status Memory::get_proc_status() { return status; }

void Memory::check_proc()
{
  if (status == process_status::FOUND_READY)
  {
	short c;
	Read<short>(baseaddr, c);

	if (c != 0x5A4D)
	{
	  status = process_status::FOUND_NO_ACCESS;
	  close_proc();
	}
  }
}

Memory::Memory() {}

int Memory::open_os()
{
  // Find and open the km driver
  if (!km::find_driver())
  {
	printf("Failed to find km driver. Make sure the driver is loaded.\n");
	return 1;
  }
  printf("km driver found.\n");
  return 0;
}

int Memory::open_proc(const char *name)
{
  // Convert name to wide string for km::find_process
  wchar_t wname[256];
  size_t converted = 0;
  mbstowcs_s(&converted, wname, 256, name, _TRUNCATE);

  INT32 pid = km::find_process(wname);
  if (pid == 0)
  {
	printf("Process %s not found.\n", name);
	status = process_status::NOT_FOUND;
	return 1;
  }

  printf("Process %s found. PID: %d\n", name, pid);

  // Fetch CR3 (Directory Table Base) for the process
  uintptr_t cr3_val = km::fetch_cr3();
  if (cr3_val)
  {
	printf("CR3: 0x%llx\n", (unsigned long long)cr3_val);
	km::cr3 = cr3_val;
  }
  else
  {
	printf("Warning: Could not fetch CR3, continuing without it.\n");
  }

  // Get process base address
  uintptr_t image_base = km::find_image();
  if (image_base == 0)
  {
	printf("Failed to get base address for %s\n", name);
	status = process_status::FOUND_NO_ACCESS;
	return 1;
  }

  baseaddr = image_base;
  printf("Base address: 0x%llx\n", (unsigned long long)baseaddr);

  // Verify MZ header
  const short MZ_HEADER = 0x5A4D;
  short header = 0;
  Read<short>(baseaddr, header);
  if (header != MZ_HEADER)
  {
	printf("MZ header check failed for %s (got 0x%X)\n", name, header);
	status = process_status::FOUND_NO_ACCESS;
	return 1;
  }

  printf("MZ header verified. Process ready.\n");
  status = process_status::FOUND_READY;
  return 0;
}

Memory::~Memory() {}

void Memory::close_proc()
{
  baseaddr = 0;
  status = process_status::NOT_FOUND;
}

uint64_t Memory::ScanPointer(uint64_t ptr_address, const uint32_t offsets[],
							 int level)
{
  if (!ptr_address)
	return 0;

  uint64_t lvl = ptr_address;

  for (int i = 0; i < level; i++)
  {
	if (!Read<uint64_t>(lvl, lvl) || !lvl)
	  return 0;
	lvl += offsets[i];
  }

  return lvl;
}

bool IsInValid(uint64_t address) {
	return address < 0x00010000 || address > 0x7FFFFFFEFFFF;
}