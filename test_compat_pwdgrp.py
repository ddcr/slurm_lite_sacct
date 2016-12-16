#!/usr/bin/env python
import unittest
import subprocess


def toSigned32(n):
    n = n & 0xffffffff
    return (n ^ 0x80000000) - 0x80000000


class MyTest(unittest.TestCase):
    def setUp(self):

        self.uid = {}
        self.gid = {}
        with open('etc/slurm/passwd.veredas', 'r') as file_object:
            for line in file_object:
                line = line.strip()
                fields = line.split(":")
                self.uid[fields[0]] = int(fields[2])

        with open('etc/slurm/group.veredas', 'r') as file_object:
            for line in file_object:
                line = line.strip()
                fields = line.split(":")
                self.gid[fields[0]] = int(fields[2])

    def test_user2uid(self):
        """
            the UID of nfsnobody is 4294967294
            (-2 if viewed as signed 32-bit int)
        """
        for user in self.uid.keys():
            cmd = "./test_compat_pwdgrp.exe -U {0:11}".format(user)
            try:
                output = subprocess.check_output(cmd,
                                                 stderr=subprocess.STDOUT,
                                                 shell=True)
            except subprocess.CalledProcessError:
                print("the call to {} ended in error".format(cmd))
            except OSError as e:
                print(e)
                raise
            uid_int = int(output.rstrip().decode('utf-8'))
            self.assertEqual(uid_int, toSigned32(self.uid[user]))

    def test_uid2user(self):
        for user, uid in self.uid.items():
            cmd = "./test_compat_pwdgrp.exe -u {0:11}".format(uid)
            try:
                output = subprocess.check_output(cmd,
                                                 stderr=subprocess.STDOUT,
                                                 shell=True)
            except subprocess.CalledProcessError:
                print("the call to {} ended in error".format(cmd))
            except OSError as e:
                print(e)
                raise
            user_int = output.rstrip().decode('utf-8')
            self.assertEqual(user_int, user)

    def test_group2gid(self):
        for group in self.gid.keys():
            cmd = "./test_compat_pwdgrp.exe -G {0:11}".format(group)
            try:
                output = subprocess.check_output(cmd,
                                                 stderr=subprocess.STDOUT,
                                                 shell=True)
            except subprocess.CalledProcessError:
                print("the call to {} ended in error".format(cmd))
            except OSError as e:
                print(e)
                raise
            gid_int = int(output.rstrip().decode('utf-8'))
            self.assertEqual(gid_int, toSigned32(self.gid[group]))

    def test_gid2group(self):
        for group, gid in self.gid.items():
            if group in ['dba', 'postgres']:
                continue
            cmd = "./test_compat_pwdgrp.exe -g {0:11}".format(gid)
            try:
                output = subprocess.check_output(cmd,
                                                 stderr=subprocess.STDOUT,
                                                 shell=True)
            except subprocess.CalledProcessError:
                print("the call to {} ended in error".format(cmd))
            except OSError as e:
                print(e)
                raise
            group_int = output.rstrip().decode('utf-8')
            self.assertEqual(group_int, group)

    def test_joinedarguments(self):
        cmd = "./test_compat_pwdgrp.exe -G {}".\
            format(','.join(self.gid.keys()))
        print(cmd)
        try:
            output = subprocess.check_output(cmd,
                                             stderr=subprocess.STDOUT,
                                             shell=True)
        except subprocess.CalledProcessError:
            print("the call to {} ended in error".format(cmd))
        except OSError as e:
            print(e)
            raise
        print(output.decode('utf-8'))


if __name__ == '__main__':
    unittest.main()
