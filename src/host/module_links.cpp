// See module_links.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "module_links.h"

#include <algorithm>

namespace host
{
	struct LinkTree::Node
	{
		explicit Node(int32_t m) : module(m), next(nullptr), branch(nullptr), closed(0), open(0), root(0), changed(0) {}

		int32_t module;
		Node *next;
		Node *branch;
		int closed;
		int open;
		int root;
		int changed;
	};

	namespace
	{
		// FUN_004c4560 (row ML-V-1, verified on listing-all-2 0x4c4560-0x4c4585): this node and every later node of its chain are marked closed, and so is everything in their branches
		template <class N>
		void CloseChain(N *node)
		{
			for (; node; node = node->next)
			{
				node->closed = 1;
				node->open = 0;

				if (node->branch)
				{
					CloseChain(node->branch);
				}
			}
		}

		// FUN_004c4650: the nodes of a chain, and of every branch in it
		template <class N>
		int CountChain(N const *node)
		{
			int count = 0;

			for (; node; node = node->next)
			{
				count++;

				if (node->branch)
				{
					count += CountChain(node->branch);
				}
			}

			return count;
		}

		// FUN_004c46c0, one chain (row ML-V-10, verified on listing-all-2 0x4c46a0-0x4c4746): every node in pre-order; place is the 1-based place of the item written
		template <class N>
		void FlattenChain(N const *node, std::vector<LinkItem> *out, int *place)
		{
			for (; node; node = node->next)
			{
				(*place)++;

				LinkItem item;

				// A node with a branch: the nodes in that branch, and the node's own place
				item.branchNo = node->branch ? CountChain(node->branch) + *place : 0;
				item.flags = 0;
				item.spare = 0;
				item.objectIndex = node->module;
				out->push_back(item);

				if (node->branch)
				{
					FlattenChain(node->branch, out, place);
				}
			}
		}
	}

	LinkTree::LinkTree(int32_t rootModule) : root(new Node(rootModule))
	{
		root->root = 1;
	}

	void LinkTree::FreeChain(Node *node)
	{
		// FUN_004c41f0: a node's next chain and its branch go; iterated along the chain so a long one can't overflow the stack
		while (node)
		{
			Node *const next = node->next;

			FreeChain(node->branch);
			delete node;
			node = next;
		}
	}

	LinkTree::~LinkTree()
	{
		FreeChain(root);
	}

	// Normal Link (FUN_004c4230, row ML-V-8, verified on listing-all-2 0x4c4230-0x4c42aa)
	bool LinkTree::Normal(int32_t module)
	{
		Node *node = root;

		root->changed = 1;

		for (;;)
		{
			if (module == node->module)
			{
				return false;
			}

			if (!node->next && (!node->branch || node->closed))
			{
				break;
			}

			Node *const old = node;

			// Into an open branch, otherwise along the chain
			node = old->branch ? (old->closed ? old->next : old->branch) : old->next;
		}

		node->next = new Node(module);
		return true;
	}

	namespace
	{
		template <class N>
		bool OpenBranchOf(N *node, int32_t module);   // FUN_004c43d0 (below)
	}

	// Branch Link (FUN_004c42b0, row ML-V-5, verified on listing-all-2 0x4c42b0-0x4c4342)
	bool LinkTree::Branch(int32_t module)
	{
		Node *node = root;

		root->changed = 1;

		if (module == node->module)
		{
			return false;
		}

		for (;;)
		{
			if (!node->next)
			{
				if (!node->branch)
				{
					break;
				}

				if (node->closed)
				{
					return false;
				}
			}

			node = (!node->branch || node->closed) ? node->next : node->branch;
		}

		node->branch = new Node(module);
		node->open = 1;

		// FUN_004c43c0 with ECX = the ROOT on this node's module (0x4c4329-0x4c4335): Open Branch walked from the root, so a branch left open on the way to this node is closed when
		// the node is met further along its chain, and the nodes after the opened one are closed (rows ML-V-2, ML-V-5)
		OpenBranchOf(root, node->module);
		return true;
	}

	namespace
	{
		// FUN_004c60a0's walk: n and every later node of its chain; a node with a branch has its module put on the END of the path while the branch is walked,
		// then the FIRST element equal to it is taken out (0x4c6272-0x4c6355)
		template <class N, class V>
		void WalkForReverse(N const *node, std::vector<int32_t> *path, std::vector<V> *out)
		{
			for (; node; node = node->next)
			{
				V visit;

				visit.module = node->module;
				visit.path = *path;
				out->push_back(visit);

				if (node->branch)
				{
					path->push_back(node->module);
					WalkForReverse(node->branch, path, out);

					std::vector<int32_t>::iterator const found = std::find(path->begin(), path->end(), node->module);

					if (found != path->end())
					{
						path->erase(found);
					}
				}
			}
		}
	}

	void LinkTree::ReverseVisits(std::vector<ReverseVisit> *out) const
	{
		if (!out)
		{
			return;
		}

		out->clear();

		std::vector<int32_t> path;

		path.push_back(root->module);
		WalkForReverse(root->next, &path, out);
	}

	// The body of FUN_004c60a0 for one path (row ML-V-12, verified on listing-all-2 0x4c60c9-0x4c626e). EBP is the node held (from the current tree's root), EBX the
	// "found or made" flag: set when the element's node is found WITHOUT a branch (0x4c624f -> 0x4c6239) or a node is made (0x4c6237-0x4c6239); a node found WITH a
	// branch moves EBP into the branch and leaves EBX as it was (0x4c6251-0x4c6253 jumps past 0x4c6239), so while it is clear the next element is looked for
	// along the branch's own chain and a new node goes on as the next of the node held.
	void LinkTree::AddReversePath(std::vector<int32_t> const &path)
	{
		Node *held = root;
		bool foundOrMade = false;

		for (size_t at = path.size(); at-- > 0;)
		{
			int32_t const wanted = path[at];

			// 0x4c60f7-0x4c6146: only while the flag is clear, and only along the chain of the node held
			if (!foundOrMade && held->next)
			{
				for (;;)
				{
					if (held->module == wanted)
					{
						break;
					}

					held = held->next;

					if (!held->next)
					{
						break;
					}
				}
			}

			if (held->module == wanted)
			{
				// 0x4c617a-0x4c6182, 0x4c624a-0x4c6253: into its branch when it has one (the flag as it was), else stay on it and set the flag
				if (held->branch)
				{
					held = held->branch;
				}
				else
				{
					foundOrMade = true;
				}
			}
			else
			{
				// 0x4c6188-0x4c6239: a new node (FUN_004c41a0: the root flag [5] on), as the branch of the node held when the flag is set (0x4c61d9), else as its
				// next (0x4c6234); it becomes the node held and the flag is set. (inteng drops an old branch it overwrites without freeing it; it is freed here.)
				Node *const made = new Node(wanted);

				made->root = 1;

				if (foundOrMade)
				{
					Node *const old = held->branch;

					held->branch = made;
					FreeChain(old);
				}
				else
				{
					Node *const old = held->next;

					held->next = made;
					FreeChain(old);
				}

				held = made;
				foundOrMade = true;
			}
		}
	}

	namespace
	{
		// FUN_004c4480 (View) and FUN_004c44f0 (Hide) (rows ML-V-3 / ML-V-4, verified on listing-all-2 0x4c4480-0x4c4555): the same walk with a different value. A node that is the module and has a branch gets its
		// "open" [4] set (View: 1, Hide: 0) and the walk stops; else the branch is searched while the node's "open" is on, then the next node.
		// (The decompile drops each recursive call's register argument; the guards before the calls, `[2] != 0 && [4] != 0` and `[1] != 0`, name the
		// branch and the next node.) Nothing else changes: not "changed", not "closed".
		template <class N>
		bool SetOpenOf(N *node, int32_t module, int value)
		{
			if (node->module == module && node->branch)
			{
				node->open = value;
				return true;
			}

			if (node->branch && node->open && SetOpenOf(node->branch, module, value))
			{
				return true;
			}

			return node->next && SetOpenOf(node->next, module, value);
		}
	}

	namespace
	{
		// FUN_004c43d0 (Open Branch; FUN_004c43c0 only calls it) (row ML-V-2, verified on listing-all-2 0x4c43d0-0x4c446d), with the register arguments of FUN_004c4560 from Apone's raw listing (apone-reads-3.md,
		// batch 2): at 0x4c4420 it is the node's NEXT, at 0x4c445c the node's BRANCH, at 0x4c43ad (Close) the node itself. The recursive calls: the branch (guard
		// `[2] != 0 && [4] != 0`) and the next node (guard `[1] != 0`), as for View.
		template <class N>
		bool OpenBranchOf(N *node, int32_t module)
		{
			if (node->module == module && node->branch)
			{
				node->open = 1;

				if (!node->closed)
				{
					return true;
				}
			}
			else
			{
				if (!(node->branch && node->open && OpenBranchOf(node->branch, module)))
				{
					// not in an open branch: along the chain
					if (!node->next || !OpenBranchOf(node->next, module))
					{
						return false;
					}

					// met in a later node: this node's branch, when open, is closed (0x4c4440-0x4c445c)
					if (node->branch && node->open)
					{
						node->open = 0;
						node->closed = 1;
						CloseChain(node->branch);
					}

					return true;
				}

				node->open = 1;   // met in this node's branch (0x4c440b)
			}

			// 0x4c4415: this node is not closed, and every later node of its chain is
			node->closed = 0;

			if (node->next)
			{
				CloseChain(node->next);
			}

			return true;
		}
	}

	bool LinkTree::Open(int32_t module)
	{
		return OpenBranchOf(root, module);
	}

	bool LinkTree::View(int32_t module)
	{
		return SetOpenOf(root, module, 1);
	}

	bool LinkTree::Hide(int32_t module)
	{
		return SetOpenOf(root, module, 0);
	}

	// Close Branch (FUN_004c4350, row ML-V-6, verified on listing-all-2 0x4c4350-0x4c43b7: FUN_004c4560 on the node itself, 0x4c43ad)
	bool LinkTree::Close(int32_t module)
	{
		Node *node = root;

		if (module == node->module)
		{
			return false;
		}

		while (node->next || (node->branch && !node->closed))
		{
			if (!node->branch || node->closed)
			{
				node = node->next;
			}
			else
			{
				if (node->module == module)
				{
					break;
				}

				node = node->branch;
			}
		}

		if (node->module == module && node->branch)
		{
			node->closed = 1;
			node->open = 0;
			CloseChain(node);
		}

		return true;
	}

	namespace
	{
		// FUN_004c4590 on one node (row ML-V-7, verified on listing-all-2 0x4c4590-0x4c463f): its (open) branch is searched, then its next chain; the node found is cut out with what hangs off it
		template <class N, class Free>
		bool DeleteIn(N *node, int32_t module, Free freeNode)
		{
			node->changed = 1;

			N *const branch = node->branch;

			if (branch && node->open)
			{
				if (branch->module == module)
				{
					N *const rest = branch->next;

					branch->next = nullptr;
					freeNode(branch);
					node->branch = rest;
					return true;
				}

				if (DeleteIn(branch, module, freeNode))
				{
					return true;
				}
			}

			N *const next = node->next;

			if (next)
			{
				if (next->module == module)
				{
					N *const rest = next->next;

					next->next = nullptr;
					freeNode(next);
					node->next = rest;
				}
				else if (DeleteIn(next, module, freeNode))
				{
					return true;
				}
			}

			// inteng gives 0 here, also after it removed the next node (FUN_004c4590, 004c4627)
			return false;
		}
	}

	bool LinkTree::Delete(int32_t module)
	{
		return DeleteIn(root, module, [](Node *gone) { FreeChain(gone); });
	}

	std::vector<LinkItem> LinkTree::Flatten() const
	{
		std::vector<LinkItem> items;
		int place = 0;

		FlattenChain(root->next, &items, &place);
		return items;
	}

	namespace
	{
		// FUN_004c40f0 (row ML-V-9, verified on listing-all-2 0x4c40c0-0x4c419e): items from `at`, `count` of them; first says the first is the head of a branch (Branch), not of the chain (Normal).
		// cursor is DAT_007043a4, the place of the item looked at (0-based). Recursion only goes as deep as the branches nest.
		template <class Tree>
		void LoadItems(Tree &tree, LinkItem const *items, int total, int at, int count, int first, int *cursor, std::function<bool(int32_t)> const &exists)
		{
			while (count > 0 && at >= 0 && at < total)
			{
				LinkItem const &item = items[at];
				bool const found = exists(item.objectIndex);

				if (found)
				{
					bool const added = first == 0 ? tree.Normal(item.objectIndex) : tree.Branch(item.objectIndex);

					(void)added;

					int const place = *cursor;

					if (item.branchNo != 0)
					{
						int const inside = item.branchNo - place;

						(*cursor)++;
						LoadItems(tree, items, total, at + 1, inside - 1, 1, cursor, exists);
						tree.Close(item.objectIndex);

						// The items the branch held are done: on to the one after them
						count = count + 1 + (place - item.branchNo);
						at += item.branchNo - place - 1;
						*cursor = item.branchNo - 1;
					}
				}

				count--;
				(*cursor)++;
				at++;
				first = 0;
			}
		}
	}

	void LinkTree::Load(LinkItem const *items, int count, std::function<bool(int32_t)> const &exists)
	{
		int cursor = 0;

		LoadItems(*this, items, count, 0, count, 0, &cursor, exists);
	}

	int32_t LinkTree::RootModule() const
	{
		return root->module;
	}

	bool LinkTree::Changed() const
	{
		return root->changed != 0;
	}

	void LinkTree::ClearChanged()
	{
		root->changed = 0;
	}

	void LinkTree::MarkChanged()
	{
		root->changed = 1;
	}

	bool LinkTree::HasLinks() const
	{
		return root->next != nullptr;
	}

	namespace
	{
		// FUN_004c0720 (row ML-V-11, verified on listing-all-2 0x4c0720-0x4c0859)
		template <class N, class T>
		void TintChain(N const *node, std::vector<T> *out)
		{
			for (; node; node = node->next)
			{
				T tint;

				tint.module = node->module;

				if (!node->branch)
				{
					tint.rgb = node->root ? 0x0000FFu : 0xFFFF00u;
				}
				else
				{
					tint.rgb = node->closed ? 0xFF0000u : 0x00FF00u;
				}

				out->push_back(tint);

				// Into the branch only while it is open
				if (node->branch && node->open)
				{
					TintChain(node->branch, out);
				}
			}
		}
	}

	std::vector<LinkTree::Tint> LinkTree::Tints() const
	{
		std::vector<Tint> tints;

		TintChain(root, &tints);
		return tints;
	}

	std::vector<int32_t> LinkTree::Modules() const
	{
		std::vector<int32_t> modules;
		std::vector<Node const *> todo;

		todo.push_back(root->next);

		while (!todo.empty())
		{
			Node const *node = todo.back();

			todo.pop_back();

			for (; node; node = node->next)
			{
				modules.push_back(node->module);

				if (node->branch)
				{
					todo.push_back(node->branch);
				}
			}
		}

		return modules;
	}

	size_t LinkTree::Count() const
	{
		return (size_t)CountChain(root->next);
	}

	bool LinkTree::Has(int32_t module) const
	{
		// Every node, closed branches too
		std::vector<Node const *> todo;

		todo.push_back(root);

		while (!todo.empty())
		{
			Node const *node = todo.back();

			todo.pop_back();

			for (; node; node = node->next)
			{
				if (node->module == module)
				{
					return true;
				}

				if (node->branch)
				{
					todo.push_back(node->branch);
				}
			}
		}

		return false;
	}
}
