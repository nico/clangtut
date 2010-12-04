class Base {
 protected:
  void function();
};

class Derived : public Base {
 public:
  using Base::function;
};

namespace a {
void function() {
}
}

namespace b = a;

void function();

void caller() {
  function();
}

void function() {
}


// TODO: templates; macros
