inherit "/inherit/tests";

private int priv_func() { return 1; }

protected varargs int prot_varargs(int a, int b) { return a + b; }

nomask void nomask_func() {}

private string *mods_of(string name, int flag) {
  foreach (mixed *f in functions(this_object(), flag)) {
    if (f[0] == name) return f;
  }
  return 0;
}

void do_tests() {
  // Without Flag 4 the return type stays bare.
  ASSERT_EQ("int", mods_of("priv_func", 3)[2]);
  ASSERT_EQ("int", mods_of("prot_varargs", 3)[2]);

  ASSERT_EQ("private int", mods_of("priv_func", 7)[2]);
#ifdef __SENSIBLE_MODIFIERS__
  ASSERT_EQ("protected varargs int", mods_of("prot_varargs", 7)[2]);
#else
  ASSERT_EQ("static varargs int", mods_of("prot_varargs", 7)[2]);
#endif
  ASSERT(strsrch(mods_of("nomask_func", 7)[2], "nomask void") != -1);

  // Arity and argument entries are unchanged by Flag 4.
  ASSERT_EQ(2, mods_of("prot_varargs", 7)[1]);
  ASSERT_EQ(sizeof(mods_of("prot_varargs", 3)), sizeof(mods_of("prot_varargs", 7)));

  // Inherited functions carry modifiers too (Flag 5 includes them).
  ASSERT(mods_of("do_tests", 5));
}
