#include "http.h"
#include <cstdlib>
#include <iomanip>

#define CHUNKSIZE 4096
#define LOG "/var/log/erss/proxy.log" // Name and path of the log

std::vector<char> handleChunked(int myfd, std::vector<char> firstbuff);

std::string computeExpire(std::string checkDate, std::string age_tmp){
  time_t date = convert_GMT(checkDate);
  time_t seconds = (time_t)atoi(age_tmp.c_str());
  time_t expireTmp = date+seconds;
  char buf[1000];
  struct tm tm_expire = *gmtime(&expireTmp);
  strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm_expire);
  std::string ans(buf);
  return ans; 
}

/*
    0 - can cache
    1 - cannot cache no-store in header
    2 - private in header
    3 - no-cache in header
*/
int checkResponse(HTTPResponse response)
{
    std::unordered_map<std::string, std::string> header = response.getheader();
    if (header.find("Cache-Control") != header.end())
    {
        std::string cache_control = header["Cache-Control"];
        //Check the cache-control field: no-store/private/no-cache
        if (cache_control.find("no-store") != std::string::npos)
        {
            return 1;
        }
        else if (cache_control.find("private") != std::string::npos)
        {
            return 2;
        }
        else if (cache_control.find("no-cache") != std::string::npos)
        {
            return 3;
        }
    }
    return 0;
}

/*
    return 
    0 - no "Expires" & "Cache-Control" header, requires validation
    1 - has "max-age" & "Date", and expired
    2 - has "max-age" & "Date", and NOT expired
    3 - has "Expires", and expired
    4 - has "Expires", and NOT expired
*/
int checkExpire(HTTPResponse response)
{
    std::unordered_map<std::string, std::string> header = response.getheader();
    if (header.find("Cache-Control") != header.end())
    {

        std::cout << "check header[Cache-Control]: " << header["Cache-Control"] << std::endl;

        std::string cache_control = header["Cache-Control"];
        //Check the cache-control field: if max-age exists, use it to check expire
        if (cache_control.find("max-age") != std::string::npos && header.find("Date") != header.end())
        {
            std::string max_age = readAge(cache_control);

            std::cout << "max_age" << max_age << std::endl;

            std::string now_time = getNow();
            std::string date = header["Date"];

            bool ifExpire = isExpire(now_time, date, max_age);
            if (ifExpire) return 1;
            else return 2;
        }
    }

    //if expire exists, use it to check, it is used to validate
    if (header.find("Expires") != header.end())
    {
        std::string now_time = getNow();

        std::cout << "check header[Expires]: " << header["Expires"] << std::endl;

        std::string expiretime = header["Expires"];

        bool ifExpire = isExpire(now_time, expiretime);
        if (ifExpire) return 3;
        else return 4;
    }
    return 0;
}

std::vector<char> myrecv(int myfd)
{
    std::vector<char> tempbuf;
    std::vector<char> tempseg(TEMPSIZE);

    while (1)
    {
        //std::cout << "In myrecv loop" << std::endl;
        int segsize = recv(myfd, &tempseg.data()[0], TEMPSIZE, 0);

        //std::cout << "Segsize: " << segsize << std::endl;

        if (segsize < 0)
        {
            std::cout << "Error in receive request" << std::endl;
            close(myfd);
            exit(1);
        }

        for (int i = 0; i < segsize; i++)
        {
            tempbuf.push_back(tempseg[i]);
        }

        //std::cout << "Tempbuf size: " << tempbuf.size() << std::endl;
        /*
            May need to change to std::fill
        */
        std::fill(tempseg.begin(), tempseg.end(), '\0');

        if (segsize < TEMPSIZE || (segsize == TEMPSIZE && tempbuf[tempbuf.size() - 1] == '\0'))
        { // Finish receive
            tempbuf.push_back('\0');
            HTTPResponse tempres(tempbuf);
            std::unordered_map<std::string, std::string> tempheader = tempres.getheader();
            if (tempheader.find("Content-Length") == tempheader.end())
            {
                // handle chunked
                if (tempheader.find("Transfer-Encoding") != tempheader.end() && tempheader["Transfer-Encoding"] == "chunked")
                {
                    if (DEVELOPMENT)
                        std::cout << "\nChunked here!\n";
                    tempbuf.pop_back();
                    return handleChunked(myfd, tempbuf);
                }
                else
                {
                    break;
                }
            }
            std::string tempbody = tempres.getBody();
            //std::cout << "Body size in buffer: " << tempbody.size() << std::endl;
            //std::cout << "Content-Length: " << tempheader["Content-Length"] << std::endl;
            if (tempbody.size() >= std::stoi(tempheader["Content-Length"]) - 1)
            {
                break;
            }
            else
            {
                //                std::cout << "OK, print whole temp buffer: " << std::endl;
                // std::cout << tempbuf.data() << std::endl;
            }
        }

        if (segsize == 0)
        {
            tempbuf.push_back('\0');
            break;
        }
    }

    std::cout << "Final tempbuf size: " << tempbuf.size() << std::endl;

    return tempbuf;
}

std::vector<char> handleChunked(int myfd, std::vector<char> firstbuff)
{
    int length = 0;
    std::vector<char> tempseg(CHUNKSIZE);
    std::vector<char> tempchunk;

    firstbuff.push_back('\0');
    if (DEVELOPMENT)
        std::cout << "Response buffer: " << firstbuff.data() << std::endl
                  << std::endl;
    firstbuff.pop_back();

    // firstbuff doesn't contain '\0'
    while (1)
    {
        int recvsize = recv(myfd, &tempseg.data()[0], CHUNKSIZE, 0);

        for (int i = 0; i < recvsize; i++)
        {
            tempchunk.push_back(tempseg[i]);
        }

        std::fill(tempseg.begin(), tempseg.end(), '\0');

        if (recvsize == CHUNKSIZE)
            continue;

        tempchunk.push_back('\0');

        std::cout << "Temp chunk size: " << tempchunk.size() << std::endl;
        std::cout << "Temp chunk content: " << tempchunk.data() << std::endl;

        // handle chunk
        std::string chunkstr = tempchunk.data();
        size_t linebreak = chunkstr.find("\r\n");
        std::cout << "Linebreak: " << linebreak << std::endl;
        int chunk_length = std::stoi(chunkstr.substr(0, linebreak), nullptr, 16);

        length += chunk_length;

        // last chunk
        if (chunk_length == 0)
        {
            // before break, add '\0'
            firstbuff.push_back('\0');

            // Receive trailer
            while (1)
            {
                int trailersize = recv(myfd, &tempseg.data()[0], CHUNKSIZE, 0);
                std::fill(tempseg.begin(), tempseg.end(), '\0');
                if (trailersize == CHUNKSIZE)
                    continue;
                break;
            }

            // Finish content read, assembly
            HTTPResponse tempres(firstbuff);
            std::unordered_map<std::string, std::string> realHeader = tempres.accessHeader();

            // Erase chunk header & add content-length header
            realHeader.erase("Transfer-Encoding");
            realHeader["Content-Length"] = std::to_string(length);

            // Update temporary response's buffer
            tempres.reParse();

            return tempres.getBuffer();
        }
        // else parse content to firstbuff
        else
        {
            chunkstr.erase(0, linebreak + 2);
            for (int i = 0; i < chunk_length; i++)
            {
                firstbuff.push_back(tempchunk[i]);
            }
            tempchunk.clear();
        }
    }
}

void handlehttp(int reqfd)
{
    std::ofstream log(LOG, std::ios::app);

    // Get request
    std::vector<char> tempbuf = myrecv(reqfd);

    if (DEVELOPMENT)
        std::cout << "Temporary buffer got " << tempbuf.size() << " bytes." << std::endl
                  << "The content is: " << std::endl
                  << tempbuf.data() << std::endl;

    HTTPRequest newreq(tempbuf);

    // Handle request
    newreq.handlereq(reqfd);

    close(reqfd);
}

std::string readAge(std::string control)
{
    std::string ans;

    // find the position that number starts
    size_t age = control.find("max-age=");
    age += 8;
    size_t colon = control.find(",", age);
    if (colon == std::string::npos)
        ans = control.erase(0, age);
    else
        ans = control.substr(age, colon - age);

    if (DEVELOPMENT)
        std::cout << "Age is " << ans << " seconds.\n"
                  << std::endl;
    return ans;
}

std::string getNow()
{
    char buf[1000];
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    strftime(buf, sizeof buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    printf("Time is: [%s]\n", buf);

    std::string ans(buf);
    return ans;
}

int convert_week(std::string weekday)
{
    std::vector<std::string> week_to_int;
    week_to_int.push_back("Sun");
    week_to_int.push_back("Mon");
    week_to_int.push_back("Tue");
    week_to_int.push_back("Wed");
    week_to_int.push_back("Thu");
    week_to_int.push_back("Fri");
    week_to_int.push_back("Sat");

    for (int i = 0; i < week_to_int.size(); i++)
    {
        if (week_to_int[i].compare(weekday) == 0)
        {
            return i;
        }
    }
    return 7;
}

int convert_month(std::string month)
{
    std::vector<std::string> month_to_int;
    month_to_int.push_back("Jan");
    month_to_int.push_back("Feb");
    month_to_int.push_back("Mar");
    month_to_int.push_back("Apr");
    month_to_int.push_back("May");
    month_to_int.push_back("Jun");
    month_to_int.push_back("Jul");
    month_to_int.push_back("Aug");
    month_to_int.push_back("Sep");
    month_to_int.push_back("Oct");
    month_to_int.push_back("Nov");
    month_to_int.push_back("Dec");
    for (int i = 0; i < month_to_int.size(); i++)
    {
        if (month_to_int[i].compare(month) == 0)
        {
            return i;
        }
    }
    return 12;
}

time_t convert_GMT(std::string s)
{
    //initiate to have right format
    time_t rawtime;
    time(&rawtime);
    struct tm *tm1 = localtime(&rawtime);
    std::string week, date, month, year, current_time;
    //parse the input string
    //get week
    size_t pos = s.find(", ");
    if (pos != std::string::npos)
    {
        week = s.substr(0, pos);
    }
    s.erase(0, pos + 2);

    //get date
    pos = s.find(" ");
    if (pos != std::string::npos)
    {
        date = s.substr(0, pos);
    }
    s.erase(0, pos + 1);

    //get month
    pos = s.find(" ");
    if (pos != std::string::npos)
    {
        month = s.substr(0, pos);
    }
    s.erase(0, pos + 1);

    //get year
    pos = s.find(" ");
    if (pos != std::string::npos)
    {
        year = s.substr(0, pos);
    }
    s.erase(0, pos + 1);

    //get current time
    pos = s.find(" ");
    if (pos != std::string::npos)
    {
        current_time = s.substr(0, pos);
    }
    s.erase(0, pos + 1);
    int weekint = convert_week(week);
    if (weekint == 7)
    {
        std::cerr << "wrong format of week" << std::endl;
    }
    int dateint = atoi(date.c_str());
    int monthint = convert_month(month);
    if (monthint == 12)
    {
        std::cerr << "wrong format of month" << std::endl;
    }
    int yearint = atoi(year.c_str()) - 1900;

    //parse time format
    std::string temp;
    pos = current_time.find(":");
    if (pos != std::string::npos)
    {
        temp = current_time.substr(0, pos);
        tm1->tm_hour = atoi(temp.c_str());
    }
    current_time.erase(0, pos + 1);

    pos = current_time.find(":");
    if (pos != std::string::npos)
    {
        temp = current_time.substr(0, pos);
        tm1->tm_min = atoi(temp.c_str());
    }
    current_time.erase(0, pos + 1);

    //store the info
    tm1->tm_sec = atoi(current_time.c_str());
    tm1->tm_wday = weekint;
    tm1->tm_mday = dateint;
    tm1->tm_isdst = 0;
    time_t timeData = mktime(tm1);
    return timeData;
}

bool isExpire(std::string now, std::string date, std::string seconds)
{
    time_t now_time = convert_GMT(now);
    time_t date_time = convert_GMT(date);
    if (difftime(now_time, date_time) > (double)atoi(seconds.c_str()))
    {
        return true;
    }
    return false;
}

bool isExpire(std::string now, std::string date)
{
    time_t now_time = convert_GMT(now);
    time_t date_time = convert_GMT(date);
    if (difftime(now_time, date_time) > 0)
    {
        return true;
    }
    return false;
}
