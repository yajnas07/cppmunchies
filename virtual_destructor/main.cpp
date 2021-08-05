#include<iostream>
using namespace std;
class base_without_vd {
    public:
    string name;
    base_without_vd(string & nm):name(nm){
        cout << name << ":Inside base_without class constructor" << endl;
    }
    virtual ~base_without_vd() {
        cout << "base_without_vd destructor" << endl;
    }
};

class base_with_vd {
    public:
    string name;
    base_with_vd(string & nm):name(nm){
        cout << name << ":Inside base_with_vd class constructor" << endl;
    }
    virtual ~base_with_vd() {
        cout << "base_with_vd destructor" << endl;
    }
};

template <typename T>
class derived_tmpt 
    : public base_without_vd
    , public base_with_vd
{
private:
    T data;

public:
    derived_tmpt(string nm, T val) : base_without_vd(nm), base_with_vd(nm), data(val) {
        cout << "Inside constructor, value is:" << data << endl;
    }
    ~derived_tmpt() {
        cout << "Derived destructor" << endl;
    }
};

int main(int argc,char * argv[])
{

    base_without_vd * bool_ptr = new derived_tmpt<bool>("bool_ptr",true); 
    base_with_vd * int_ptr = new derived_tmpt<int>("int_ptr",100); 
    base_with_vd * float_ptr = new derived_tmpt<float>("float_ptr",10.6); 

    cout << "deleting bool_ptr, this will not delete complete object" << endl;
    delete dynamic_cast< derived_tmpt<bool>* >(bool_ptr);
    cout << "deleting int_ptr, this will delete complete object" << endl;
    delete int_ptr;
    cout << "deleting float_ptr, this will delete complete object" << endl;
    delete float_ptr;

}
