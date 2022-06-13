#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <unistd.h>


using namespace std;
/*******************************************************************************/
class Producer;
class CoEditor;
class BoundedQueue;
class UnBoundedQueue;
class Dispatcher;
class ScreenManager;
/*******************************************************************************/
vector<BoundedQueue*> articles;
BoundedQueue* newsArticles;
BoundedQueue* sportArticles;
BoundedQueue* weatherArticles;
UnBoundedQueue* screenQ;
vector<Producer*> producers;
vector<CoEditor*> editors;
Dispatcher* dispatcher;
ScreenManager* screenManager;
/*******************************************************************************/
class Semaphore {
private:
    mutex mutex_;
    condition_variable condition_;
    unsigned long count_ = 0;
public:
    Semaphore(int count) : count_(count){}

    void release(){
        lock_guard<decltype(mutex_)> lock(mutex_);
        ++count_;
        condition_.notify_one();
    }

    void acquire(){
        unique_lock<decltype(mutex_)> lock(mutex_);
        while (!count_)
            condition_.wait(lock);
        --count_;
    }
};
class UnBoundedQueue {

    Semaphore *f;
    mutex m;

public:
    queue <string>q;
    UnBoundedQueue() : f(new Semaphore(0)){};
    void push(string a) {
        m.lock();
        q.push(a);
        m.unlock();
        f->release();
    }
    string pop(){
        f->acquire();
        m.lock();
        string a = q.front();
        q.pop();
        m.unlock();
        return  a;
    }
    ~UnBoundedQueue(){
        delete(f);
        f = nullptr;
    }
};

class BoundedQueue {

    Semaphore *f;
    Semaphore* e;
    mutex m;

public:
    queue <string>q;
    BoundedQueue(size_t size) : f(new Semaphore(0)), e(new Semaphore(size)){};
    void push(string a) {
        e->acquire();
        m.lock();
        q.push(a);
        m.unlock();
        f->release();
    }
    string pop(){
        f->acquire();
        m.lock();
        string a = q.front();
        q.pop();
        m.unlock();
        e->release();
        return a;
    }
    ~BoundedQueue(){
        delete(f);
        f = nullptr;
        delete(e);
        e = nullptr;
    }
};

class Producer{
public:
    string name;
    int numOfP;
    int qSize;
    BoundedQueue* q;
    int n;
    int s;
    int w;
    Producer(string name, int numOfP, int qSize){
        this->name = name;
        this->numOfP = numOfP;
        this->qSize = qSize;
        q = new BoundedQueue(qSize);
        articles.push_back(q);

    }
    void produce(){
        for(int i = 0; i < numOfP; ++i){
            int r = rand() %3;
            switch (r) {
                case 0:
                    q->push(name + " NEWS " + to_string(n++));
                    break;
                case 1:
                    q->push(name + " SPORTS " + to_string( s++));
                    break;
                case 2:
                    q->push(name + " WEATHER " + to_string( w++));
                    break;

            }
        }
        q->push("DONE");

    }
    ~Producer(){
        delete(q);
        q = nullptr;
    }
};
class CoEditor {
private:
    BoundedQueue* q;
public:
    CoEditor(BoundedQueue* q) : q(q){}
    void edit(){
        while (true){
            string a = q->pop();
            if(a == "DONE"){
                screenQ->push(a);
                break;
            }
            screenQ->push(a);
        }
    }
};
class Dispatcher {

public:
    void dispatch() {
        while(!articles.empty()){
            for(int i = 0; i < articles.size(); ++i){
                string s = articles[i]->pop();
                if(s == "DONE") {
                    articles.erase(articles.begin() + i);
                }
                else if(s.find("NEWS") != std::string::npos){
                    newsArticles->push(s);
                }
                else if(s.find("SPORTS") != std::string::npos){
                    sportArticles->push(s);
                }
                else if(s.find("WEATHER") != std::string::npos){
                    weatherArticles->push(s);
                }
            }
        }
        newsArticles->push("DONE");
        sportArticles->push("DONE");
        weatherArticles->push("DONE");

    }
};

class ScreenManager {
public:
    void print(){
        int counter = 0;
        while (true){

            string a = screenQ->pop();
            if(a == "DONE"){
                ++counter;
                if(counter == 3){
                    cout<<"DONE"<<endl;
                    return;
                }
            } else{
                cout<<a<<endl;
            }

        }
    }

};
int readConFile(int argc, char *argv[]) {
    if(argc != 2){
        exit(1);
    }
    fstream confile;
    int ceqz;
    confile.open(argv[1], ios::in); //open a file to perform read operation using file object
    if (confile.is_open()){   //checking whether the file is open
        string line;
        string pname;
        int nump;
        int qsize;
        while(getline(confile, line)){ //read data from file object and put it into string.

            if(getline(confile, line)){
                pname = "PRODUCER " + line;
            }
            if(getline(confile, line)){
                nump = stoi(line);
            } else{
                ceqz = stoi(line);
                break;
            }
            if(getline(confile, line)){
                qsize = stoi(line);
            }
            producers.push_back(new Producer(pname, nump, qsize));

        }

        confile.close(); //close the file object.
    }
    else{
        cout<<"conf file didnt open"<<endl;
        return -1;
    }
    newsArticles = new BoundedQueue(ceqz);
    sportArticles = new BoundedQueue(ceqz);
    weatherArticles = new BoundedQueue(ceqz);
    editors.push_back(new CoEditor(newsArticles));
    editors.push_back(new CoEditor(sportArticles));
    editors.push_back(new CoEditor(weatherArticles));

    dispatcher = new Dispatcher();
    screenManager = new ScreenManager();
    screenQ = new UnBoundedQueue();

    return 0;

}

int start(){
    vector<thread> threads;

    for(auto p : producers){
        threads.emplace_back(thread(&Producer::produce, p));
    }
    threads.emplace_back(thread(&Dispatcher::dispatch, dispatcher));
    for(auto e : editors){
        threads.emplace_back(thread(&CoEditor::edit, e));
    }
    threads.emplace_back(thread(&ScreenManager::print, screenManager));
    for (auto& th : threads) {
        th.join();
    }
    return 0;
}
void end(){
    for(auto p: producers){
        delete(p);
        p = nullptr;
    }
    for(auto e : editors){
        delete(e);
        e = nullptr;
    }
    for(auto a : articles){
        delete(a);
        a = nullptr;
    }
    delete(dispatcher);
    dispatcher = nullptr;
    delete(screenManager);
    screenManager = nullptr;
    delete(screenQ);
    screenQ = nullptr;
    delete(newsArticles);
    newsArticles = nullptr;
    delete(sportArticles);
    sportArticles = nullptr;
    delete(weatherArticles);
    weatherArticles = nullptr;
}

int main(int argc, char *argv[]) {
    if(readConFile(argc, argv)){ return -1;}
    start();
    end();

    return 0;
}
