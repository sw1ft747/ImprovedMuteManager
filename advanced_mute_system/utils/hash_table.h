// C++
// Hash Table

#pragma once

#include <stdio.h>
#include <inttypes.h>

//-----------------------------------------------------------------------------

typedef void (*fnAddEntryFailed)(void *pEntry, void *pValue);
typedef bool (*fnOnRemoveEntry)(void *pEntry, void *pValue);
typedef void (*fnIterateEntries)(void *pEntry);

//-----------------------------------------------------------------------------

template <class hashData>
class CHashEntry64
{
public:
	CHashEntry64() : key(0), value(0), next(NULL) { }

	uint64_t key;
	hashData value;

	CHashEntry64 *next;
};

//-----------------------------------------------------------------------------

template <uint32_t tableSize = 32, class hashData = uint32_t>
class CHashTable64
{
public:
	CHashTable64();
	~CHashTable64();

	CHashEntry64<hashData> *GetEntry(uint64_t key);

	bool AddEntry(uint64_t key, hashData value, fnAddEntryFailed pfnCallback = NULL);

	bool RemoveEntry(uint64_t key, hashData value = NULL, fnOnRemoveEntry pfnCallback = NULL);

	void IterateEntries(fnIterateEntries pfnCallback);

	void RemoveAll();

	int GetTableSize() const;

private:
	uint32_t Hash(uint8_t *key, size_t length) const;

private:
	CHashEntry64<hashData> *m_table[tableSize];
};

//-----------------------------------------------------------------------------

template <uint32_t tableSize, class hashData>
CHashTable64<tableSize, hashData>::CHashTable64() : m_table()
{
}

template <uint32_t tableSize, class hashData>
inline CHashTable64<tableSize, hashData>::~CHashTable64()
{
	RemoveAll();
}

template <uint32_t tableSize, class hashData>
CHashEntry64<hashData> *CHashTable64<tableSize, hashData>::GetEntry(uint64_t key)
{
	uint32_t hash = Hash((uint8_t *)&key, sizeof(uint64_t));
	int index = hash % tableSize;

	CHashEntry64<hashData> *entry = m_table[index];

	while (entry)
	{
		if (entry->key == key)
			return entry;

		entry = entry->next;
	}

	return NULL;
}

template <uint32_t tableSize, class hashData>
bool CHashTable64<tableSize, hashData>::AddEntry(uint64_t key, hashData value, fnAddEntryFailed pfnCallback /* = NULL */)
{
	uint32_t hash = Hash((uint8_t *)&key, sizeof(uint64_t));
	int index = hash % tableSize;

	CHashEntry64<hashData> *prev = NULL;
	CHashEntry64<hashData> *entry = m_table[index];

	while (entry)
	{
		if (entry->key == key)
		{
			if (pfnCallback)
				pfnCallback(reinterpret_cast<void *>(entry), reinterpret_cast<void *>(&value));

			return false;
		}

		prev = entry;
		entry = entry->next;
	}

	CHashEntry64<hashData> *newEntry = new CHashEntry64<hashData>;

	newEntry->key = key;
	newEntry->value = value;

	if (m_table[index])
		prev->next = newEntry;
	else
		m_table[index] = newEntry;

	return true;
}

template <uint32_t tableSize, class hashData>
bool CHashTable64<tableSize, hashData>::RemoveEntry(uint64_t key, hashData value /* = NULL */, fnOnRemoveEntry pfnCallback /* = NULL */)
{
	uint32_t hash = Hash((uint8_t *)&key, sizeof(uint64_t));
	int index = hash % tableSize;

	CHashEntry64<hashData> *prev = NULL;
	CHashEntry64<hashData> *entry = m_table[index];

	while (entry)
	{
		if (entry->key == key)
		{
			if (pfnCallback && !pfnCallback(reinterpret_cast<void *>(entry), reinterpret_cast<void *>(&value)))
				return false;

			if (entry->next)
			{
				if (prev)
					prev->next = entry->next;
				else
					m_table[index] = entry->next;
			}
			else
			{
				if (prev)
					prev->next = NULL;
				else
					m_table[index] = NULL;
			}

			delete entry;

			return true;
		}

		prev = entry;
		entry = entry->next;
	}

	return false;
}

template <uint32_t tableSize, class hashData>
void CHashTable64<tableSize, hashData>::IterateEntries(fnIterateEntries pfnCallback)
{
	for (int i = 0; i < tableSize; ++i)
	{
		CHashEntry64<hashData> *entry = m_table[i];

		while (entry)
		{
			pfnCallback(reinterpret_cast<void *>(entry));

			entry = entry->next;
		}
	}
}

template <uint32_t tableSize, class hashData>
void CHashTable64<tableSize, hashData>::RemoveAll()
{
	for (int i = 0; i < tableSize; ++i)
	{
		CHashEntry64<hashData> *entry = m_table[i];

		while (entry)
		{
			CHashEntry64<hashData> *remove_entry = entry;

			entry = entry->next;

			delete remove_entry;
		}

		m_table[i] = NULL;
	}
}

template <uint32_t tableSize, class hashData>
inline int CHashTable64<tableSize, hashData>::GetTableSize() const
{
	return tableSize;
}

template <uint32_t tableSize, class hashData>
__forceinline uint32_t CHashTable64<tableSize, hashData>::Hash(uint8_t *key, size_t length) const
{
	// Jenkins hash function

	size_t i = 0;
	uint32_t hash = 0;

	while (i != length)
	{
		hash += key[i++];
		hash += hash << 10;
		hash ^= hash >> 6;
	}

	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;

	return hash;
}