#include<iostream>
#include<vector>
#include<algorithm>
using namespace std;

//Define a predicate function
bool is_shorter_string(const string&lhs, const string&rhs) {
    return lhs.size() < rhs.size();
}
//Define a functor class
class is_longer {
    public:
    bool operator()(const string&lhs, const string&rhs) {
    return lhs.size() > rhs.size();
    }
};


int main(int argc,char * argv[])
{

vector<string> dictonary;
string input="";
cout << "Enter few words. End woth word 'done'" << endl;
while((cin >> input) && (input != "done")) {
    dictonary.push_back(input);
}

//Sort with default built in predicate
cout << "Words in sorted order(like dictionary)" << endl;
sort(dictonary.begin(),dictonary.end());
for(int i=0 ; i < dictonary.size(); i++){
    cout << dictonary[i] << " ";
}
cout << endl;

//Sort with function predicate
sort(dictonary.begin(),dictonary.end(),is_shorter_string);
cout << "\nWords sorted as size" << endl;
for(int i=0 ; i < dictonary.size(); i++){
    cout << dictonary[i] << " ";
}
cout << endl;

//Sort with functor predicate
sort(dictonary.begin(),dictonary.end(),is_longer());
cout << "\nWords sorted as size, bigger ones first" << endl;
for(int i=0 ; i < dictonary.size(); i++){
    cout << dictonary[i] << " ";
}
cout << endl;
}
