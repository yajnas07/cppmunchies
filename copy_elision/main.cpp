#include<iostream>
using namespace std;

class copiable_class {
public:
    copiable_class() {
        cout << "Inside constructor" << endl;
    }
    copiable_class(const copiable_class & other){
        cout << "Copying.." << endl;
    }

};

//Invokes copy constructor, Named return value optimization 
copiable_class creator_nrvo()
{
    copiable_class instance;
    return instance;
}

//Invokes copy constructor, return value optimization
copiable_class creator_rvo()
{
    return copiable_class();
}


int main(int argc,char * argv[])
{
    //Copying should have been printed two times
    //But compiler optimizes out call to copy constructor for temporary objects
    copiable_class instance1 =  creator_rvo();
    copiable_class instance2 =  creator_nrvo();
}
