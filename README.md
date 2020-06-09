# Web-Crawler
Multithreaded Web crawler that reads 1M webpages from an input file and downloads unique IPs that include a robots.txt file 

Runs n threads that download unique IPs and then parses the downloaded HTML file to display server/page statistics.

Command line arguments:
- Number of threads
- Input file

Usage: `Web-Crawl 3500 URL-input.txt`
