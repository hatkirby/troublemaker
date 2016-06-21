#include <twitter.h>
#include <mutex>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>
#include <list>
#include <yaml-cpp/yaml.h>
#include <fstream>

struct userstats {
  int total = 0;
  int days = 1;
  int day1 = 0;
  int day2 = 0;
  int day3 = 0;
  int day4 = 0;
  int day5 = 0;
  int day6 = 0;
  int day7 = 0;
};

int main(int argc, char** argv)
{
  srand(time(NULL));
  rand(); rand(); rand(); rand();
  
  YAML::Node config = YAML::LoadFile("config.yml");
    
  twitter::auth auth;
  auth.setConsumerKey(config["consumer_key"].as<std::string>());
  auth.setConsumerSecret(config["consumer_secret"].as<std::string>());
  auth.setAccessKey(config["access_key"].as<std::string>());
  auth.setAccessSecret(config["access_secret"].as<std::string>());
  
  std::set<twitter::user_id> friends;
  std::mutex friends_mutex;
  
  std::map<twitter::user_id, userstats> stats;
  std::mutex stats_mutex;
  
  // Read in old data
  {
    std::ifstream datafile("data.txt");
    
    if (datafile.is_open())
    {
      std::string line;
      while (std::getline(datafile, line))
      {
        std::istringstream iss(line);
        
        twitter::user_id uid;
        iss >> uid;
        
        userstats& s = stats[uid];
        iss >> s.total;
        iss >> s.days;
        iss >> s.day2;
        iss >> s.day3;
        iss >> s.day4;
        iss >> s.day5;
        iss >> s.day6;
        iss >> s.day7;
      }
    }
  }
  
  twitter::client client(auth);
  client.setUserStreamNotifyCallback([&] (twitter::notification n) {
    std::lock_guard<std::mutex> friend_guard(friends_mutex);
    
    if (n.getType() == twitter::notification::type::friends)
    {
      friends = n.getFriends();
    } else if (n.getType() == twitter::notification::type::follow)
    {
      friends.insert(n.getUser().getID());
    } else if (n.getType() == twitter::notification::type::unfollow)
    {
      friends.erase(n.getUser().getID());
    } else if (n.getType() == twitter::notification::type::tweet)
    {
      if (
        (friends.count(n.getTweet().getAuthor().getID()) == 1) // Only monitor people you are following
        && (!n.getTweet().isRetweet()) // Ignore retweets
        && (n.getTweet().getText().front() != '@') // Ignore messages
      )
      {
        std::lock_guard<std::mutex> stats_guard(stats_mutex);
        
        userstats& s = stats[n.getTweet().getAuthor().getID()];
        s.total++;
        s.day1++;
        
        if (s.days >= 7)
        {
          int outof = s.total;
          if (outof < 200)
          {
            outof = 200;
          }
          
          if (rand() % outof == 0)
          {
            std::cout << "@" << n.getTweet().getAuthor().getScreenName() << "'s one of " << outof << "!" << std::endl;
            
            std::string doc = client.generateReplyPrefill(n.getTweet()) + "is this a subtweet?";
            twitter::tweet theTweet;
            twitter::response resp = client.updateStatus(doc, theTweet, n.getTweet());
            if (resp != twitter::response::ok)
            {
              std::cout << "Error tweeting witty joke: " << resp << std::endl;
            }
          }
        }
      }
    } else if (n.getType() == twitter::notification::type::followed)
    {
      twitter::response resp = client.follow(n.getUser());
      if (resp != twitter::response::ok)
      {
        std::cout << "Twitter error while following @" << n.getUser().getScreenName() << ": " << resp << std::endl;
      }
    }
  });
  
  std::this_thread::sleep_for(std::chrono::minutes(1));
  
  std::cout << "Starting streaming" << std::endl;
  client.startUserStream();
  
  for (;;)
  {
    std::this_thread::sleep_for(std::chrono::hours(24));
    
    // Unfollow people who have unfollowed us
    std::set<twitter::user_id> friends;
    std::set<twitter::user_id> followers;
    twitter::response resp = client.getFriends(friends);
    if (resp == twitter::response::ok)
    {
      resp = client.getFollowers(followers);
      if (resp == twitter::response::ok)
      {
        std::list<twitter::user_id> old_friends, new_followers;
        std::set_difference(std::begin(friends), std::end(friends), std::begin(followers), std::end(followers), std::back_inserter(old_friends));
        std::set_difference(std::begin(followers), std::end(followers), std::begin(friends), std::end(friends), std::back_inserter(new_followers));
        
        for (auto f : old_friends)
        {
          std::lock_guard<std::mutex> friend_guard(friends_mutex);
          friends.erase(f);
          
          resp = client.unfollow(f);
          if (resp != twitter::response::ok)
          {
            std::cout << "Twitter error while unfollowing" << std::endl;
          }
        }
        
        for (auto f : new_followers)
        {
          resp = client.follow(f);
          if (resp != twitter::response::ok)
          {
            std::cout << "Twitter error while following" << std::endl;
          }
        }
      } else {
        std::cout << "Twitter error while getting followers: " << resp << std::endl;
      }
    } else {
      std::cout << "Twitter error while getting friends: " << resp << std::endl;
    }
    
    std::cout << "stat rotation" << std::endl;
    // This is all just for stats rotation
    {
      std::lock_guard<std::mutex> stats_guard(stats_mutex);
      std::ofstream datafile("data.txt", std::ofstream::out | std::ofstream::trunc);
      
      for (auto& mapping : stats)
      {
        auto& s = mapping.second;
        if (s.days < 7)
        {
          s.days++;
        }
        
        s.day7 = s.day6;
        s.day6 = s.day5;
        s.day5 = s.day4;
        s.day4 = s.day3;
        s.day3 = s.day2;
        s.day2 = s.day1;
        s.day1 = 0;
        
        s.total = s.day2 + s.day3 + s.day4 + s.day5 + s.day6 + s.day7;
        
        datafile << mapping.first << " ";
        datafile << s.total << " ";
        datafile << s.days << " ";
        datafile << s.day2 << " ";
        datafile << s.day3 << " ";
        datafile << s.day4 << " ";
        datafile << s.day5 << " ";
        datafile << s.day6 << " ";
        datafile << s.day7 << std::endl;
      }
    }
  }
}
