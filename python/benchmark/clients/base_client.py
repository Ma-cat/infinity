from abc import abstractmethod
from typing import Any
import subprocess
import os
import time
import logging
import random
import json
import h5py
import numpy as np
import threading


class BaseClient:
    """
    Base class for all clients(Qdrant, ES, infinity).
    mode is a string that corresponds to a JSON file's address in the configurations.
    Each client reads the required parameters from the JSON configuration file.
    """

    def __init__(self, conf_path: str) -> None:
        """
        The conf_path configuration file is parsed to extract the needed parameters, which are then all stored for use by other functions.
        """
        self.data = None
        self.queries = list()
        self.clients = list()
        self.lock = threading.Lock()
        self.next_begin = 0
        self.results = []
        self.done_queries = 0
        self.active_threads = 0

    @abstractmethod
    def upload(self):
        """
        Upload data and build indexes (parameters are parsed by __init__).
        """
        pass

    @abstractmethod
    def setup_clients(self, num_threads=1):
        pass

    @abstractmethod
    def do_single_query(self, query_id, client_id) -> list[Any]:
        pass

    def download_data(self, url, target_path):
        """
        Download dataset and extract it into path.
        """
        _, ext = os.path.splitext(url)
        if ext == ".bz2":
            bz2_path = target_path + ".bz2"
            subprocess.run(["wget", "-O", bz2_path, url], check=True)
            subprocess.run(["bunzip2", bz2_path], check=True)
            extracted_path = os.path.splitext(bz2_path)[0]
            os.rename(extracted_path, target_path)
        else:
            subprocess.run(["wget", "-O", target_path, url], check=True)

    def search(self, is_express=False, num_threads=1):
        self.setup_clients(num_threads)

        query_path = os.path.join(self.path_prefix, self.data["query_path"])
        _, ext = os.path.splitext(query_path)
        if self.data["mode"] == "fulltext":
            assert ext == ".txt"
            for line in open(query_path, "r"):
                line = line.strip()
                self.queries.append(line)
        else:
            self.data["mode"] == "vector"
            if ext == ".hdf5":
                with h5py.File(query_path, "r") as f:
                    self.queries = list(f["test"])
            else:
                assert ext == "jsonl"
                for line in open(query_path, "r"):
                    query = json.loads(line)["vector"]
                    self.queries.append(query)

        self.active_threads = num_threads
        threads = []
        for i in range(num_threads):
            threads.append(
                threading.Thread(
                    target=self.search_thread_mainloop,
                    args=[is_express, i],
                    daemon=True,
                )
            )
        for i in range(num_threads):
            threads[i].start()

        report_qps_sec = 60
        sleep_sec = 10
        sleep_cnt = 0
        done_warm_up = True
        if is_express:
            logging.info(f"Let database warm-up for {report_qps_sec} seconds")
            done_warm_up = False
        start = time.time()
        start_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(start))
        report_prev = start
        done_queries_prev = 0
        done_queries_curr = 0

        while self.active_threads > 0:
            time.sleep(sleep_sec)
            sleep_cnt += 1
            if sleep_cnt < report_qps_sec / sleep_sec:
                continue
            sleep_cnt = 0
            now = time.time()
            if done_warm_up:
                with self.lock:
                    done_queries_curr = self.done_queries
                avg_start = done_queries_curr / (now - start)
                avg_interval = (done_queries_curr - done_queries_prev) / (
                    now - report_prev
                )
                done_queries_prev = done_queries_curr
                report_prev = now
                logging.info(
                    f"average QPS since {start_str}: {avg_start}, average QPS of last interval:{avg_interval}"
                )
            else:
                with self.lock:
                    self.done_queries = 0
                start = now
                start_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(start))
                report_prev = now
                done_warm_up = True
                logging.info(
                    "Collecting statistics for 30 minutes. Print statistics so far every minute. Type Ctrl+C to quit."
                )

        for i in range(num_threads):
            threads[i].join()
        if not is_express:
            self.save_and_check_results(self.results)

    def search_thread_mainloop(self, is_express: bool, client_id: int):
        query_batch = 100
        num_queries = len(self.queries)
        if is_express:
            local_rng = random.Random()  # random number generator per thread
            deadline = time.time() + 30 * 60  # 30 minutes
            while time.time() < deadline:
                for i in range(query_batch):
                    query_id = local_rng.randrange(0, num_queries)
                    _ = self.do_single_query(query_id, client_id)
                with self.lock:
                    self.done_queries += query_batch
        else:
            begin = 0
            end = 0
            local_results = list()
            while end < num_queries:
                with self.lock:
                    self.done_queries += end - begin
                    begin = self.next_begin
                    end = begin + query_batch
                    if end > num_queries:
                        end = num_queries
                    self.next_begin = end
                for query_id in range(begin, end):
                    start = time.time()
                    result = self.do_single_query(query_id, client_id)
                    latency = (time.time() - start) * 1000
                    result = [(query_id, latency)] + result
                    local_results.append(result)
            with self.lock:
                self.done_queries += end - begin
                self.results += local_results
        with self.lock:
            self.active_threads -= 1

    def save_and_check_results(self, results: list[list[Any]]):
        """
        Compare the search results with ground truth to calculate recall.
        """
        self.results.sort(key=lambda x: x[0][0])
        if "result_path" in self.data:
            result_path = self.data["result_path"]
            with open(result_path, "w") as f:
                for result in results:
                    line = json.dumps(result)
                    f.write(line + "\n")
            logging.info("query_result_path: {0}".format(result_path))
        latencies = []
        for result in results:
            latencies.append(result[0][1])
        logging.info(
            f"""mean_time: {np.mean(latencies)}, std_time: {np.std(latencies)},
                max_time: {np.max(latencies)}, min_time: {np.min(latencies)},
                p95_time: {np.percentile(latencies, 95)}, p99_time: {np.percentile(latencies, 99)}"""
        )
        if "ground_truth_path" in self.data:
            ground_truth_path = self.data["ground_truth_path"]
            _, ext = os.path.splitext(ground_truth_path)
            precisions = []
            if self.data["mode"] == "vector":
                assert ext == ".hdf5"
                with h5py.File(ground_truth_path, "r") as f:
                    expected_result = f["neighbors"]
                    for i, result in enumerate(results):
                        ids = [((x >> 32) << 23) + (x & 0xFFFFFFFF) for x in result[1:]]
                        precision = (
                            len(set(ids).intersection(expected_result[i][1:]))
                            / self.data["topK"]
                        )
                        precisions.append(precision)
            else:
                assert ext == ".json" or ext == ".jsonl"
                with open(ground_truth_path, "r") as f:
                    for i, line in enumerate(f):
                        expected_result = json.loads(line)
                        exp_ids = [
                            ((x[0] >> 32) << 23) + (x[0] & 0xFFFFFFFF)
                            for x in expected_result[1:]
                        ]
                        result = results[i]
                        ids = [
                            ((x[0] >> 32) << 23) + (x[0] & 0xFFFFFFFF)
                            for x in result[1:]
                        ]
                        precision = (
                            len(set(ids).intersection(exp_ids)) / self.data["topK"]
                        )
                        precisions.append(precision)
            logging.info(f"""mean_precisions: {np.mean(precisions)}""")

    def run_experiment(self, args):
        """
        run experiment and save results.
        """
        if args.import_data:
            start_time = time.time()
            self.upload()
            finish_time = time.time()
            logging.info(f"upload finish, cost time = {finish_time - start_time}")
        elif args.query >= 1:
            self.search(is_express=False, num_threads=args.query)
        elif args.query_express >= 1:
            self.search(is_express=True, num_threads=args.query_express)
