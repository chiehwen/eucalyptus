// -*- mode: C; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil -*-
// vim: set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:

/*************************************************************************
 * Copyright 2009-2012 Eucalyptus Systems, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 *
 * Please contact Eucalyptus Systems, Inc., 6755 Hollister Ave., Goleta
 * CA 93117, USA or visit http://www.eucalyptus.com/licenses/ if you need
 * additional information or have any questions.
 *
 * This file may incorporate work covered under the following copyright
 * and permission notice:
 *
 *   Software License Agreement (BSD License)
 *
 *   Copyright (c) 2008, Regents of the University of California
 *   All rights reserved.
 *
 *   Redistribution and use of this software in source and binary forms,
 *   with or without modification, are permitted provided that the
 *   following conditions are met:
 *
 *     Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *     Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer
 *     in the documentation and/or other materials provided with the
 *     distribution.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *   FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *   COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *   INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *   BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE. USERS OF THIS SOFTWARE ACKNOWLEDGE
 *   THE POSSIBLE PRESENCE OF OTHER OPEN SOURCE LICENSED MATERIAL,
 *   COPYRIGHTED MATERIAL OR PATENTED MATERIAL IN THIS SOFTWARE,
 *   AND IF ANY SUCH MATERIAL IS DISCOVERED THE PARTY DISCOVERING
 *   IT MAY INFORM DR. RICH WOLSKI AT THE UNIVERSITY OF CALIFORNIA,
 *   SANTA BARBARA WHO WILL THEN ASCERTAIN THE MOST APPROPRIATE REMEDY,
 *   WHICH IN THE REGENTS' DISCRETION MAY INCLUDE, WITHOUT LIMITATION,
 *   REPLACEMENT OF THE CODE SO IDENTIFIED, LICENSING OF THE CODE SO
 *   IDENTIFIED, OR WITHDRAWAL OF THE CODE CAPABILITY TO THE EXTENT
 *   NEEDED TO COMPLY WITH ANY SUCH LICENSES OR RIGHTS.
 ************************************************************************/

//!
//! @file node/handlers_default.c
//! This implements the default operations handlers supported by all hypervisor.
//!

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                  INCLUDES                                  |
 |                                                                            |
\*----------------------------------------------------------------------------*/

#define _FILE_OFFSET_BITS 64    // so large-file support works on 32-bit systems
#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU               /* strnlen */
#include <string.h>             /* strlen, strcpy */
#include <time.h>
#ifndef __DARWIN_UNIX03
#include <limits.h>             /* INT_MAX */
#else /* ! _DARWIN_C_SOURCE */
#include <i386/limits.h>
#endif /* ! _DARWIN_C_SOURCE */
#include <sys/types.h>          /* fork */
#include <sys/wait.h>           /* waitpid */
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>
#include <pthread.h>
#include <signal.h>             /* SIGINT */
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>

#include <eucalyptus.h>
#include <ipc.h>
#include <misc.h>
#include <backing.h>
#include <vnetwork.h>
#include <euca_auth.h>
#include <vbr.h>
#include <iscsi.h>
#include <sensor.h>
#include <windows-bundle.h>
#include <euca_string.h>

#include "handlers.h"
#include "xml.h"
#include "hooks.h"

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                  DEFINES                                   |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                  TYPEDEFS                                  |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                ENUMERATIONS                                |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                 STRUCTURES                                 |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                             EXTERNAL VARIABLES                             |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/* Should preferably be handled in header file */

// coming from handlers.c
extern sem *hyp_sem;
extern sem *inst_sem;
extern sem *inst_copy_sem;
extern bunchOfInstances *global_instances;
extern bunchOfInstances *global_instances_copy;

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                              GLOBAL VARIABLES                              |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                              STATIC VARIABLES                              |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                             EXTERNAL PROTOTYPES                            |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/* Should preferably be handled in header file */

extern int update_disk_aliases(ncInstance * instance);  // defined in handlers.c

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                             EXPORTED PROTOTYPES                            |
 |                                                                            |
\*----------------------------------------------------------------------------*/

int find_and_terminate_instance(struct nc_state_t *nc_state, ncMetadata * pMeta, char *instanceId, int force, ncInstance ** instance_p, char destroy);

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                              STATIC PROTOTYPES                             |
 |                                                                            |
\*----------------------------------------------------------------------------*/

static int doInitialize(struct nc_state_t *nc);
static int doRunInstance(struct nc_state_t *nc, ncMetadata * pMeta, char *uuid, char *instanceId, char *reservationId, virtualMachine * params,
                         char *imageId, char *imageURL, char *kernelId, char *kernelURL, char *ramdiskId, char *ramdiskURL, char *ownerId,
                         char *accountId, char *keyName, netConfig * netparams, char *userData, char *launchIndex, char *platform, int expiryTime,
                         char **groupNames, int groupNamesSize, ncInstance ** outInst);
static int doRebootInstance(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId);
static int doGetConsoleOutput(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, char **consoleOutput);
static int doTerminateInstance(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, int force, int *shutdownState, int *previousState);
static int doDescribeInstances(struct nc_state_t *nc, ncMetadata * pMeta, char **instIds, int instIdsLen, ncInstance *** outInsts, int *outInstsLen);
static int doDescribeResource(struct nc_state_t *nc, ncMetadata * pMeta, char *resourceType, ncResource ** outRes);
static int doAssignAddress(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, char *publicIp);
static int doPowerDown(struct nc_state_t *nc, ncMetadata * pMeta);
static int doStartNetwork(struct nc_state_t *nc, ncMetadata * pMeta, char *uuid, char **remoteHosts, int remoteHostsLen, int port, int vlan);
static int xen_detach_helper(struct nc_state_t *nc, char *instanceId, char *localDevReal, char *xml);
static int doAttachVolume(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, char *volumeId, char *remoteDev, char *localDev);
static int doDetachVolume(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, char *volumeId, char *remoteDev, char *localDev, int force,
                          int grab_inst_sem);
static void change_createImage_state(ncInstance * instance, createImage_progress state);
static int cleanup_createImage_task(ncInstance * instance, struct createImage_params_t *params, instance_states state, createImage_progress result);
static void *createImage_thread(void *arg);
static int doCreateImage(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, char *volumeId, char *remoteDev);
static void change_bundling_state(ncInstance * instance, bundling_progress state);
static int restart_instance(ncInstance * instance);
static int cleanup_bundling_task(ncInstance * instance, struct bundling_params_t *params, bundling_progress result);
static void *bundling_thread(void *arg);
static int doBundleInstance(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, char *bucketName, char *filePrefix, char *walrusURL,
                            char *userPublicKey, char *S3Policy, char *S3PolicySig);
static int doBundleRestartInstance(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId);
static int doCancelBundleTask(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId);
static int doDescribeBundleTasks(struct nc_state_t *nc, ncMetadata * pMeta, char **instIds, int instIdsLen, bundleTask *** outBundleTasks,
                                 int *outBundleTasksLen);
static int doDescribeSensors(struct nc_state_t *nc, ncMetadata * pMeta, int historySize, long long collectionIntervalTimeMs, char **instIds,
                             int instIdsLen, char **sensorIds, int sensorIdsLen, sensorResource *** outResources, int *outResourcesLen);

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                              CALLBACK STRUCTURE                            |
 |                                                                            |
\*----------------------------------------------------------------------------*/

//! Default LIBVIRT operation handlers
struct handlers default_libvirt_handlers = {
    .name = "default",
    .doInitialize = doInitialize,
    .doDescribeInstances = doDescribeInstances,
    .doRunInstance = doRunInstance,
    .doTerminateInstance = doTerminateInstance,
    .doRebootInstance = doRebootInstance,
    .doGetConsoleOutput = doGetConsoleOutput,
    .doDescribeResource = doDescribeResource,
    .doStartNetwork = doStartNetwork,
    .doAssignAddress = doAssignAddress,
    .doPowerDown = doPowerDown,
    .doAttachVolume = doAttachVolume,
    .doDetachVolume = doDetachVolume,
    .doCreateImage = doCreateImage,
    .doBundleInstance = doBundleInstance,
    .doBundleRestartInstance = doBundleRestartInstance,
    .doCancelBundleTask = doCancelBundleTask,
    .doDescribeBundleTasks = doDescribeBundleTasks,
    .doDescribeSensors = doDescribeSensors,
};

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                                   MACROS                                   |
 |                                                                            |
\*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*\
 |                                                                            |
 |                               IMPLEMENTATION                               |
 |                                                                            |
\*----------------------------------------------------------------------------*/

//!
//! Default hypervisor initialize handler
//!
//! @param[in] nc a pointer to the NC state structure
//!
//! @return Always returns EUCA_OK for now
//!
static int doInitialize(struct nc_state_t *nc)
{
    return EUCA_OK;
}

//!
//! Default hypervisor handler to run instances
//!
//! @param[in]  nc a pointer to the NC state structure
//! @param[in]  pMeta a pointer to the node controller (NC) metadata structure
//! @param[in]  uuid unique user identifier string
//! @param[in]  instanceId the instance identifier string (i-XXXXXXXX)
//! @param[in]  reservationId the reservation identifier string
//! @param[in]  params a pointer to the virtual machine parameters to use
//! @param[in]  imageId UNUSED
//! @param[in]  imageURL UNUSED
//! @param[in]  kernelId the kernel image identifier (eki-XXXXXXXX)
//! @param[in]  kernelURL the kernel image URL address
//! @param[in]  ramdiskId the ramdisk image identifier (eri-XXXXXXXX)
//! @param[in]  ramdiskURL the ramdisk image URL address
//! @param[in]  ownerId the owner identifier string
//! @param[in]  accountId the account identifier string
//! @param[in]  keyName the key name string
//! @param[in]  netparams a pointer to the network parameters string
//! @param[in]  userData the user data string
//! @param[in]  launchIndex the launch index string
//! @param[in]  platform the platform name string
//! @param[in]  expiryTime the reservation expiration time
//! @param[in]  groupNames a list of group name string
//! @param[in]  groupNamesSize the number of group name in the groupNames list
//! @param[out] outInstPtr the list of instances created by this request
//!
//! @return EUCA_OK on success or proper error code. Known error code returned include: EUCA_ERROR
//!         EUCA_MEMORY_ERROR and EUCA_THREAD_ERROR
//!
static int doRunInstance(struct nc_state_t *nc, ncMetadata * pMeta, char *uuid, char *instanceId, char *reservationId, virtualMachine * params,
                         char *imageId, char *imageURL, char *kernelId, char *kernelURL, char *ramdiskId, char *ramdiskURL, char *ownerId,
                         char *accountId, char *keyName, netConfig * netparams, char *userData, char *launchIndex, char *platform, int expiryTime,
                         char **groupNames, int groupNamesSize, ncInstance ** outInstPtr)
{
    int ret = EUCA_OK;
    ncInstance *instance = NULL;
    netConfig ncnet = { 0 };

    *outInstPtr = NULL;

    memcpy(&ncnet, netparams, sizeof(netConfig));

    // check as much as possible before forking off and returning
    sem_p(inst_sem);
    instance = find_instance(&global_instances, instanceId);
    sem_v(inst_sem);
    if (instance) {
        if (instance->state == TEARDOWN) {
            // fully cleaned up, so OK to revive it, e.g., with euca-start-instance
            remove_instance(&global_instances, instance);
            free_instance(&instance);
        } else {
            logprintfl(EUCAERROR, "[%s] instance already running\n", instanceId);
            return EUCA_ERROR;  //! @todo return meaningful error codes?
        }
    }
    if (!
        (instance =
         allocate_instance(uuid, instanceId, reservationId, params, instance_state_names[PENDING], PENDING, pMeta->userId, ownerId, accountId, &ncnet,
                           keyName, userData, launchIndex, platform, expiryTime, groupNames, groupNamesSize))) {
        logprintfl(EUCAERROR, "[%s] could not allocate instance struct\n", instanceId);
        return EUCA_MEMORY_ERROR;
    }
    instance->launchTime = time(NULL);

    // parse and sanity-check the virtual boot record
    if (vbr_parse(&(instance->params), pMeta) != EUCA_OK) {
        ret = EUCA_ERROR;
        goto error;
    }

    change_state(instance, STAGING);

    sem_p(inst_sem);
    int error = add_instance(&global_instances, instance);
    copy_instances();
    sem_v(inst_sem);
    if (error) {
        logprintfl(EUCAERROR, "[%s] could not save instance struct\n", instanceId);
        ret = EUCA_ERROR;
        goto error;
    }
    // do the potentially long tasks in a thread
    pthread_attr_t *attr = (pthread_attr_t *) EUCA_ZALLOC(1, sizeof(pthread_attr_t));
    if (!attr) {
        logprintfl(EUCAERROR, "[%s] out of memory\n", instanceId);
        ret = EUCA_MEMORY_ERROR;
        goto error;
    }
    pthread_attr_init(attr);
    pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&(instance->tcb), attr, startup_thread, (void *)instance)) {
        pthread_attr_destroy(attr);
        logprintfl(EUCAERROR, "[%s] failed to spawn a VM startup thread\n", instanceId);
        sem_p(inst_sem);
        remove_instance(&global_instances, instance);
        copy_instances();
        sem_v(inst_sem);
        EUCA_FREE(attr);
        ret = EUCA_THREAD_ERROR;
        goto error;
    }
    pthread_attr_destroy(attr);
    EUCA_FREE(attr);

    *outInstPtr = instance;
    return EUCA_OK;

error:
    free_instance(&instance);
    return (ret);
}

//!
//! Unsupported reboot instance operation. Needs to be redefined for each hypervisor.
//!
//! @param[in] nc a pointer to the NC state structure
//! @param[in] pMeta a pointer to the node controller (NC) metadata structure
//! @param[in] instanceId the instance identifier string (i-XXXXXXXX)
//!
//! @return Always return EUCA_FATAL_ERROR
//!
static int doRebootInstance(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId)
{
    logprintfl(EUCAERROR, "[%s] no default for %s!\n", instanceId, __func__);
    return (EUCA_UNSUPPORTED_ERROR);
}

//!
//! Unsupported get console output operation. Needs to be redefined for each hypervisor.
//!
//! @param[in]  nc a pointer to the NC state structure
//! @param[in]  pMeta a pointer to the node controller (NC) metadata structure
//! @param[in]  instanceId the instance identifier string (i-XXXXXXXX)
//! @param[out] consoleOutput a pointer to the unallocated string that will contain the output
//!
//! @return Always return EUCA_FATAL_ERROR
//!
static int doGetConsoleOutput(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, char **consoleOutput)
{
    logprintfl(EUCAERROR, "[%s] no default for %s!\n", instanceId, __func__);
    return (EUCA_UNSUPPORTED_ERROR);
}

//!
//! finds instance by ID and destroys it on the hypervisor
//! NOTE: this must be called with inst_sem semaphore held
//!
//! @param[in]  nc_state a pointer to the NC state structure
//! @param[in]  pMeta a pointer to the node controller (NC) metadata structure
//! @param[in]  instanceId the instance identifier string (i-XXXXXXXX)
//! @param[in]  force if set to 0 will force the termination of an instance.
//! @param[out] instance_p a pointer to the instance matching the input instance identifier
//! @param[in]  destroy if set to 1, the domain will be destroyed. If set to 0, the domain will only be shutdown.
//!
//! @return EUCA_OK on success or proper error code. Known error code returned include: EUCA_NOT_FOUND_ERROR.
//!
int find_and_terminate_instance(struct nc_state_t *nc_state, ncMetadata * pMeta, char *instanceId, int force, ncInstance ** instance_p, char destroy)
{
    ncInstance *instance = NULL;
    virConnectPtr *conn = NULL;
    int err = 0;
    int i = 0;

    instance = find_instance(&global_instances, instanceId);
    if (instance == NULL)
        return EUCA_NOT_FOUND_ERROR;
    *instance_p = instance;

    // detach all attached volumes
    for (i = 0; i < EUCA_MAX_VOLUMES; ++i) {
        ncVolume *volume = &instance->volumes[i];
        if (!is_volume_used(volume))
            continue;

        int ret;
        logprintfl(EUCAINFO, "[%s] detaching volume %s, force=%d on termination\n", instanceId, volume->volumeId, force);
        if (nc_state->H->doDetachVolume) {
            ret = nc_state->H->doDetachVolume(nc_state, pMeta, instanceId, volume->volumeId, volume->remoteDev, volume->localDevReal, 0, 0);
        } else {
            ret = nc_state->D->doDetachVolume(nc_state, pMeta, instanceId, volume->volumeId, volume->remoteDev, volume->localDevReal, 0, 0);
        }

        // do our best to detach, then proceed
        if ((ret != EUCA_OK)) {
            if (nc_state->H->doDetachVolume) {
                ret = nc_state->H->doDetachVolume(nc_state, pMeta, instanceId, volume->volumeId, volume->remoteDev, volume->localDevReal, 1, 0);
            } else {
                ret = nc_state->D->doDetachVolume(nc_state, pMeta, instanceId, volume->volumeId, volume->remoteDev, volume->localDevReal, 1, 0);
            }
        }

        if ((ret != EUCA_OK) && (force == 0)) {
            logprintfl(EUCAWARN, "[%s] detaching of volume on terminate failed\n", instanceId);
            // return ret;
        }
    }

    // try stopping the domain
    conn = check_hypervisor_conn();
    if (conn) {
        sem_p(hyp_sem);
        virDomainPtr dom = virDomainLookupByName(*conn, instanceId);
        sem_v(hyp_sem);
        if (dom) {
            // protect 'destroy' commands as we do with 'create' because we've seen problems during concurrent libvirt invocations
            sem_p(hyp_sem);
            if (destroy)
                err = virDomainDestroy(dom);
            else
                err = virDomainShutdown(dom);
            sem_v(hyp_sem);
            if (err == 0) {
                if (destroy)
                    logprintfl(EUCAINFO, "[%s] destroying instance\n", instanceId);
                else
                    logprintfl(EUCAINFO, "[%s] shutting down instance\n", instanceId);
            }

            sem_p(hyp_sem);
            {
                virDomainFree(dom); //! @todo necessary?
            }
            sem_v(hyp_sem);
        } else {
            if (instance->state != BOOTING && instance->state != STAGING && instance->state != TEARDOWN)
                logprintfl(EUCAWARN, "[%s] instance to be terminated not running on hypervisor\n", instanceId);
        }
    }
    return EUCA_OK;
}

//!
//! Finds and terminate an instance.
//!
//! @param[in]  nc a pointer to the NC state structure
//! @param[in]  pMeta a pointer to the node controller (NC) metadata structure
//! @param[in]  instanceId the instance identifier string (i-XXXXXXXX)
//! @param[in]  force if set to 1 will force the termination of the instance
//! @param[out] shutdownState the instance state code after the call to find_and_terminate_instance() if successful
//! @param[out] previousState the instance state code after the call to find_and_terminate_instance() if successful
//!
//! @return EUCA_OK on success or proper error code from find_and_terminate_instance().
//!
//! @see find_and_terminate_instance()
//!
static int doTerminateInstance(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, int force, int *shutdownState, int *previousState)
{
    ncInstance *instance = NULL;
    int err = EUCA_ERROR;
    char resourceName[1][MAX_SENSOR_NAME_LEN] = { {0} };
    char resourceAlias[1][MAX_SENSOR_NAME_LEN] = { {0} };

    euca_strncpy(resourceName[0], instanceId, MAX_SENSOR_NAME_LEN);
    sensor_refresh_resources(resourceName, resourceAlias, 1);   // refresh stats so latest instance measurements are captured before it disappears

    sem_p(inst_sem);
    err = find_and_terminate_instance(nc, pMeta, instanceId, force, &instance, 1);
    if (err != EUCA_OK) {
        copy_instances();
        sem_v(inst_sem);
        return err;
    }
    // change the state and let the monitoring_thread clean up state
    if (instance->state != TEARDOWN && instance->state != CANCELED) {
        // do not leave TEARDOWN (cleaned up) or CANCELED (already trying to terminate)
        if (instance->state == STAGING) {
            change_state(instance, CANCELED);
        } else {
            change_state(instance, SHUTOFF);
        }
    }
    copy_instances();
    sem_v(inst_sem);

    //! @todo Chuck needs to find out if this should be moved at the begining.
    *previousState = instance->stateCode;
    *shutdownState = instance->stateCode;

    return EUCA_OK;
}

//!
//! Finds and retrieves information in regards to instances
//!
//! @param[in]  nc a pointer to the NC state structure
//! @param[in]  pMeta a pointer to the node controller (NC) metadata structure
//! @param[in]  instIds a pointer the list of instance identifiers to retrieve data for
//! @param[in]  instIdsLen the number of instance identifiers in the instIds list
//! @param[out] outInsts a pointer the list of instances for which we have data
//! @param[out] outInstsLen the number of instances in the outInsts list.
//!
//! @return EUCA_OK on success or proper error code. Known error code returned include: EUCA_MEMORY_ERROR.
//!
static int doDescribeInstances(struct nc_state_t *nc, ncMetadata * pMeta, char **instIds, int instIdsLen, ncInstance *** outInsts, int *outInstsLen)
{
    ncInstance *instance = NULL;
    ncInstance *tmp = NULL;
    int total = 0;
    int i = 0;
    int j = 0;
    int k = 0;

    logprintfl(EUCADEBUG, "invoked userId=%s correlationId=%s epoch=%d services[0]{.name=%s .type=%s .uris[0]=%s}\n",
               SP(pMeta->userId), SP(pMeta->correlationId), pMeta->epoch, SP(pMeta->services[0].name), SP(pMeta->services[0].type),
               SP(pMeta->services[0].uris[0]));

    *outInstsLen = 0;
    *outInsts = NULL;

    sem_p(inst_copy_sem);
    if (instIdsLen == 0)        // describe all instances
        total = total_instances(&global_instances_copy);
    else
        total = instIdsLen;

    *outInsts = EUCA_ZALLOC(total, sizeof(ncInstance *));
    if ((*outInsts) == NULL) {
        sem_v(inst_copy_sem);
        return EUCA_MEMORY_ERROR;
    }

    k = 0;
    for (i = 0; (instance = get_instance(&global_instances_copy)) != NULL; i++) {
        // only pick ones the user (or admin) is allowed to see
        if (strcmp(pMeta->userId, nc->admin_user_id)
            && strcmp(pMeta->userId, instance->userId))
            continue;

        if (instIdsLen > 0) {
            for (j = 0; j < instIdsLen; j++)
                if (!strcmp(instance->instanceId, instIds[j]))
                    break;

            if (j >= instIdsLen)
                // instance of no relevance right now
                continue;
        }
        // (* outInsts)[k++] = instance;
        tmp = (ncInstance *) EUCA_ALLOC(1, sizeof(ncInstance));
        memcpy(tmp, instance, sizeof(ncInstance));
        (*outInsts)[k++] = tmp;
    }
    *outInstsLen = k;
    sem_v(inst_copy_sem);

    return EUCA_OK;
}

//!
//! Describe the resources status for this component
//!
//! @param[in]  nc a pointer to the NC state structure
//! @param[in]  pMeta a pointer to the node controller (NC) metadata structure
//! @param[in]  resourceType UNUSED
//! @param[out] outRes a list of resources we retrieved data for
//!
//! @return EUCA_OK on success or proper error code. Known error code returned include: EUCA_OVERFLOW_ERROR,
//!         EUCA_MEMORY_ERROR and EUCA_ERROR.
//!
static int doDescribeResource(struct nc_state_t *nc, ncMetadata * pMeta, char *resourceType, ncResource ** outRes)
{
    ncResource *res = NULL;
    ncInstance *inst = NULL;

    // stats to re-calculate now
    long long mem_free = 0;
    long long disk_free = 0;
    int cores_free = 0;

    // intermediate sums
    long long sum_mem = 0;      // for known domains: sum of requested memory
    long long sum_disk = 0;     // for known domains: sum of requested disk sizes
    int sum_cores = 0;          // for known domains: sum of requested cores

    *outRes = NULL;
    sem_p(inst_copy_sem);
    while ((inst = get_instance(&global_instances_copy)) != NULL) {
        if (inst->state == TEARDOWN)
            continue;           // they don't take up resources
        sum_mem += inst->params.mem;
        sum_disk += (inst->params.disk);
        sum_cores += inst->params.cores;
    }
    sem_v(inst_copy_sem);

    disk_free = nc->disk_max - sum_disk;
    if (disk_free < 0)
        disk_free = 0;          // should not happen

    mem_free = nc->mem_max - sum_mem;
    if (mem_free < 0)
        mem_free = 0;           // should not happen

    cores_free = nc->cores_max - sum_cores; //! @todo should we -1 for dom0?
    if (cores_free < 0)
        cores_free = 0;         // due to timesharing

    // check for potential overflow - should not happen
    if (nc->mem_max > INT_MAX || mem_free > INT_MAX || nc->disk_max > INT_MAX || disk_free > INT_MAX) {
        logprintfl(EUCAERROR, "stats integer overflow error (bump up the units?)\n");
        logprintfl(EUCAERROR, "   memory: max=%-10lld free=%-10lld\n", nc->mem_max, mem_free);
        logprintfl(EUCAERROR, "     disk: max=%-10lld free=%-10lld\n", nc->disk_max, disk_free);
        logprintfl(EUCAERROR, "    cores: max=%-10lld free=%-10d\n", nc->cores_max, cores_free);
        logprintfl(EUCAERROR, "       INT_MAX=%-10d\n", INT_MAX);
        return EUCA_OVERFLOW_ERROR;
    }

    res = allocate_resource("OK", nc->iqn, nc->mem_max, mem_free, nc->disk_max, disk_free, nc->cores_max, cores_free, "none");
    if (res == NULL) {
        logprintfl(EUCAERROR, "out of memory\n");
        return EUCA_MEMORY_ERROR;
    }

    *outRes = res;
    logprintfl(EUCADEBUG, "returning cores=%d/%lld mem=%lld/%lld disk=%lld/%lld iqn=%s\n", cores_free, nc->cores_max, mem_free, nc->mem_max,
               disk_free, nc->disk_max, nc->iqn);
    return EUCA_OK;
}

//!
//! Assigns a public IP address to an instance
//!
//! @param[in] nc a pointer to the NC state structure
//! @param[in] pMeta a pointer to the node controller (NC) metadata structure
//! @param[in] instanceId the instance identifier string (i-XXXXXXXX)
//! @param[in] publicIp a string representation of the public IP to assign to the instance
//!
//! @return EUCA_OK on success or proper error code. Known error code returned include: EUCA_INVALID_ERROR.
//!
static int doAssignAddress(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, char *publicIp)
{
    ncInstance *instance = NULL;

    if (instanceId == NULL || publicIp == NULL) {
        logprintfl(EUCAERROR, "[%s] internal error (bad input parameters to doAssignAddress)\n", instanceId);
        return (EUCA_INVALID_ERROR);
    }

    sem_p(inst_sem);
    {
        instance = find_instance(&global_instances, instanceId);
        if (instance) {
            snprintf(instance->ncnet.publicIp, 24, "%s", publicIp);
        }
        save_instance_struct(instance);
        copy_instances();
    }
    sem_v(inst_sem);

    return EUCA_OK;
}

//!
//! Powers down this component to save some $$$.
//!
//! @param[in] nc a pointer to the NC state structure
//! @param[in] pMeta a pointer to the node controller (NC) metadata structure
//!
//! @return Always return EUCA_OK.
//!
static int doPowerDown(struct nc_state_t *nc, ncMetadata * pMeta)
{
    char cmd[MAX_PATH] = { 0 };
    int rc = 0;

    snprintf(cmd, MAX_PATH, "%s /usr/sbin/powernap-now", nc->rootwrap_cmd_path);
    logprintfl(EUCADEBUG, "saving power: %s\n", cmd);
    rc = system(cmd);
    rc = rc >> 8;
    if (rc)
        logprintfl(EUCAERROR, "cmd failed: %d\n", rc);

    return EUCA_OK;
}

//!
//! Starts the network process.
//!
//! @param[in] nc a pointer to the NC state structure
//! @param[in] pMeta a pointer to the node controller (NC) metadata structure
//! @param[in] uuid a string containing the user unique identifier (UNUSED)
//! @param[in] remoteHosts the list of remote hosts (UNUSED)
//! @param[in] remoteHostsLen the number of hosts in the remoteHosts list (UNUSED)
//! @param[in] port the port number to use for the network (UNUSED)
//! @param[in] vlan the network vlan to use.
//!
//! @return the error codes from vnetStartNetwork()
//!
//! @see vnetStartNetwork()
//!
static int doStartNetwork(struct nc_state_t *nc, ncMetadata * pMeta, char *uuid, char **remoteHosts, int remoteHostsLen, int port, int vlan)
{
    int rc = 0;
    int ret = EUCA_ERROR;
    char *brname = NULL;

    rc = vnetStartNetwork(nc->vnetconfig, vlan, NULL, NULL, NULL, &brname);
    if (rc) {
        ret = EUCA_ERROR;
        logprintfl(EUCAERROR, "failed to start network (port=%d vlan=%d return=%d)\n", port, vlan, rc);
    } else {
        ret = EUCA_OK;
        logprintfl(EUCAINFO, "started network (port=%d vlan=%d)\n", port, vlan);
    }

    // Regardless of the error code, we should always check and free brname
    EUCA_FREE(brname);
    return (ret);
}

//!
//! because libvirt detach has a bug in some version (which one?) this helper detaches
//! volumes using 'virsh', bypassing libvirt
//!
//! @param[in] nc a pointer to the NC state structure
//! @param[in] instanceId the instance identifier string (i-XXXXXXXX)
//! @param[in] localDevReal the target device
//! @param[in] xml the XML file path this will save to.
//!
//! @return EUCA_OK on success or proper error code. Known error code returned include: EUCA_ERROR.
//!
static int xen_detach_helper(struct nc_state_t *nc, char *instanceId, char *localDevReal, char *xml)
{
    int err = EUCA_ERROR;
    int rc = -1;

    pid_t pid = fork();
    if (!pid) {
        char tmpfile[MAX_PATH];
        snprintf(tmpfile, 32, "/tmp/detachxml.XXXXXX");
        int fd = safe_mkstemp(tmpfile);

        char devReal[32];
        char *tmp = strstr(xml, "<target");
        if (tmp == NULL) {
            logprintfl(EUCAERROR, "[%s] '<target' not found in the device xml\n", instanceId);
            return -1;
        }
        tmp = strstr(tmp, "dev=\"");
        if (tmp == NULL) {
            logprintfl(EUCAERROR, "[%s] '<target dev' not found in the device xml\n", instanceId);
            return -1;
        }
        snprintf(devReal, 32, "%s", tmp + strlen("dev=\""));
        for (int i = 0; i < 32; i++) {
            if (devReal[i] == '\"') {
                for (; i < 32; i++)
                    devReal[i] = '\0';
            }
        }

        if (fd > 0) {
            if (write(fd, xml, strlen(xml)) != strlen(xml)) {
                logprintfl(EUCAERROR, "[%s] fail to write %ld bytes in temp file.\n", instanceId, strlen(xml));
            }
            close(fd);

            char cmd[MAX_PATH];
            //! @todo does this work?
            snprintf(cmd, MAX_PATH, "[%s] executing '%s %s `which virsh` %s %s %s'", instanceId, nc->detach_cmd_path,
                     nc->rootwrap_cmd_path, instanceId, devReal, tmpfile);
            logprintfl(EUCADEBUG, "%s\n", cmd);
            rc = system(cmd);
            rc = rc >> 8;
            unlink(tmpfile);
        } else {
            logprintfl(EUCAERROR, "[%s] could not write to tmpfile for detach XML: %s\n", instanceId, tmpfile);
            rc = 1;
        }
        exit(rc);

    } else {
        // parent or failed to fork
        int status;
        rc = timewait(pid, &status, 15);
        if (WEXITSTATUS(status)) {
            logprintfl(EUCAERROR, "[%s] failed to sucessfully run detach helper\n", instanceId);
            err = EUCA_ERROR;
        } else {
            err = EUCA_OK;
        }
    }
    return (err);
}

//!
//! Attach a given volume to an instance.
//!
//! @param[in] nc a pointer to the NC state structure
//! @param[in] pMeta a pointer to the node controller (NC) metadata structure
//! @param[in] instanceId the instance identifier string (i-XXXXXXXX)
//! @param[in] volumeId the volume identifier string (vol-XXXXXXXX)
//! @param[in] remoteDev the target device name
//! @param[in] localDev the local device name
//!
//! @return EUCA_OK on success or proper error code. Known error code returned include: EUCA_ERROR,
//!         EUCA_NOT_FOUND_ERROR and EUCA_HYPERVISOR_ERROR.
//!
//! @see convert_dev_names()
//!
static int doAttachVolume(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, char *volumeId, char *remoteDev, char *localDev)
{
    int ret = EUCA_OK;
    int is_iscsi_target = 0;
    int have_remote_device = 0;
    char *xml = NULL;

    char *tagBuf;
    char *localDevName;
    char localDevReal[32], localDevTag[256], remoteDevReal[32];
    if (!strcmp(nc->H->name, "xen")) {
        tagBuf = NULL;
        localDevName = localDevReal;
    } else if (!strcmp(nc->H->name, "kvm")) {
        tagBuf = localDevTag;
        localDevName = localDevTag;
    } else {
        logprintfl(EUCAERROR, "[%s][%s] unknown hypervisor type '%s'\n", instanceId, volumeId, nc->H->name);
        return EUCA_ERROR;
    }

    // sets localDevReal to the file name from the device path
    // and, for KVM, sets localDevTag to the "unknown" string
    ret = convert_dev_names(localDev, localDevReal, tagBuf);
    if (ret)
        return ret;

    // find the instance record
    sem_p(inst_sem);
    ncInstance *instance = find_instance(&global_instances, instanceId);
    sem_v(inst_sem);
    if (instance == NULL)
        return EUCA_NOT_FOUND_ERROR;

    // try attaching to hypervisor
    virConnectPtr *conn = check_hypervisor_conn();
    if (conn == NULL) {
        logprintfl(EUCAERROR, "[%s][%s] cannot get connection to hypervisor\n", instanceId, volumeId);
        return EUCA_HYPERVISOR_ERROR;
    }
    // find domain on hypervisor
    sem_p(hyp_sem);
    virDomainPtr dom = virDomainLookupByName(*conn, instanceId);
    sem_v(hyp_sem);
    if (dom == NULL) {
        if (instance->state != BOOTING && instance->state != STAGING) {
            logprintfl(EUCAWARN, "[%s][%s] domain not running on hypervisor, cannot attach device\n", instanceId, volumeId);
        }
        return EUCA_HYPERVISOR_ERROR;
    }
    // mark volume as 'attaching'
    ncVolume *volume;
    sem_p(inst_sem);
    volume = save_volume(instance, volumeId, remoteDev, localDevName, localDevReal, VOL_STATE_ATTACHING);
    save_instance_struct(instance);
    copy_instances();
    sem_v(inst_sem);
    if (!volume) {
        logprintfl(EUCAERROR, "[%s][%s] failed to update the volume record, aborting volume attachment\n", instanceId, volumeId);
        return EUCA_ERROR;
    }
    // do iscsi connect shellout if remoteDev is an iSCSI target
    if (check_iscsi(remoteDev)) {
        char *remoteDevStr = NULL;
        is_iscsi_target = 1;

        // get credentials, decrypt them, login into target
        remoteDevStr = connect_iscsi_target(remoteDev);
        if (!remoteDevStr || !strstr(remoteDevStr, "/dev")) {
            logprintfl(EUCAERROR, "[%s][%s] failed to connect to iscsi target\n", instanceId, volumeId);
            remoteDevReal[0] = '\0';
        } else {
            logprintfl(EUCADEBUG, "[%s][%s] attached iSCSI target of host device '%s'\n", instanceId, volumeId, remoteDevStr);
            snprintf(remoteDevReal, 32, "%s", remoteDevStr);
            have_remote_device = 1;
        }
        EUCA_FREE(remoteDevStr);
    } else {
        snprintf(remoteDevReal, 32, "%s", remoteDev);
        have_remote_device = 1;
    }

    // something went wrong above, abort
    if (!have_remote_device) {
        ret = EUCA_ERROR;
        goto release;
    }
    // make sure there is a block device
    if (check_block(remoteDevReal)) {
        logprintfl(EUCAERROR, "[%s][%s] cannot verify that host device '%s' is available for hypervisor attach\n", instanceId, volumeId,
                   remoteDevReal);
        ret = EUCA_ERROR;
        goto release;
    }
    // generate XML for libvirt attachment request
    if (gen_volume_xml(volumeId, instance, localDevReal, remoteDevReal) // creates vol-XXX.xml
        || gen_libvirt_volume_xml(volumeId, instance)) {    // creates vol-XXX-libvirt.xml via XSLT transform
        logprintfl(EUCAERROR, "[%s][%s] could not produce attach device xml\n", instanceId, volumeId);
        ret = EUCA_ERROR;
        goto release;
    }
    // invoke hooks
    char path[MAX_PATH];
    char lpath[MAX_PATH];
    snprintf(path, sizeof(path), EUCALYPTUS_VOLUME_XML_PATH_FORMAT, instance->instancePath, volumeId);  // vol-XXX.xml
    snprintf(lpath, sizeof(lpath), EUCALYPTUS_VOLUME_LIBVIRT_XML_PATH_FORMAT, instance->instancePath, volumeId);    // vol-XXX-libvirt.xml
    if (call_hooks(NC_EVENT_PRE_ATTACH, lpath)) {
        logprintfl(EUCAERROR, "[%s][%s] cancelled volume attachment via hooks\n", instance->instanceId, volumeId);
        ret = EUCA_ERROR;
        goto release;
    }
    // read in libvirt XML, which may have been modified by the hook above
    if ((xml = file2str(lpath)) == NULL) {
        logprintfl(EUCAERROR, "[%s][%s] failed to read volume XML from %s\n", instance->instanceId, volumeId, lpath);
        ret = EUCA_ERROR;
        goto release;
    }
    // protect libvirt calls because we've seen problems during concurrent libvirt invocations
    // zhill - wrap with retry in case libvirt is dumb.
    int err = 0;
    for (int i = 1; i < 3; i++) {
        sem_p(hyp_sem);
        err = virDomainAttachDevice(dom, xml);
        sem_v(hyp_sem);
        if (err) {
            logprintfl(EUCAERROR, "[%s][%s] failed to attach host device '%s' to guest device '%s' on attempt %d of 3\n", instanceId, volumeId,
                       remoteDevReal, localDevReal, i);
            logprintfl(EUCADEBUG, "[%s][%s] virDomainAttachDevice() failed (err=%d) XML='%s'\n", instanceId, volumeId, err, xml);
            sleep(3);           //sleep a bit and retry.
        } else {
            break;
        }
    }

    if (err) {
        logprintfl(EUCAERROR, "[%s][%s] failed to attach host device '%s' to guest device '%s' after 3 retries\n", instanceId, volumeId,
                   remoteDevReal, localDevReal);
        logprintfl(EUCADEBUG, "[%s][%s] virDomainAttachDevice() failed (err=%d) XML='%s'\n", instanceId, volumeId, err, xml);
        ret = EUCA_ERROR;
    }

release:

    sem_p(hyp_sem);
    virDomainFree(dom);         // release libvirt resource
    sem_v(hyp_sem);

    // record volume state in memory and on disk
    char *next_vol_state;
    if (ret == EUCA_OK) {
        next_vol_state = VOL_STATE_ATTACHED;
    } else {
        next_vol_state = VOL_STATE_ATTACHING_FAILED;
    }
    sem_p(inst_sem);
    volume = save_volume(instance, volumeId, NULL, NULL, NULL, next_vol_state); // now we can record remoteDevReal
    save_instance_struct(instance);
    copy_instances();
    update_disk_aliases(instance);  // ask sensor subsystem to track the volume
    sem_v(inst_sem);
    if (volume == NULL && xml != NULL) {
        logprintfl(EUCAERROR, "[%s][%s] failed to save the volume record, aborting volume attachment (detaching)\n", instanceId, volumeId);
        sem_p(hyp_sem);
        err = virDomainDetachDevice(dom, xml);
        sem_v(hyp_sem);
        if (err) {
            logprintfl(EUCAERROR, "[%s][%s] failed to detach as part of aborting\n", instanceId, volumeId);
            logprintfl(EUCADEBUG, "[%s][%s] virDomainDetachDevice() failed (err=%d) XML='%s'\n", instanceId, volumeId, err, xml);
        }
        ret = EUCA_ERROR;
    }
    // if iSCSI and there were problems, try to disconnect the target
    if (ret != EUCA_OK && is_iscsi_target && have_remote_device) {
        logprintfl(EUCADEBUG, "[%s][%s] attempting to disconnect iscsi target due to attachment failure\n", instanceId, volumeId);
        if (disconnect_iscsi_target(remoteDev) != 0) {
            logprintfl(EUCAERROR, "[%s][%s] failed to disconnect iscsi target\n", instanceId, volumeId);
        }
    }

    if (ret == EUCA_OK) {
        logprintfl(EUCAINFO, "[%s][%s] volume attached as host device '%s' to guest device '%s'\n", instanceId, volumeId, remoteDevReal,
                   localDevReal);
    }
    // remoteDev can be a long string, so to keep log readable, we log it at TRACE level unless there was a problem
    int log_level_for_devstring = EUCATRACE;
    if (ret != EUCA_OK)
        log_level_for_devstring = EUCADEBUG;
    logprintfl(log_level_for_devstring, "[%s][%s] remote device string: %s\n", instanceId, volumeId, remoteDev);

    EUCA_FREE(xml);

    return ret;
}

//!
//! Detach a given volume from an instance.
//!
//! @param[in] nc a pointer to the NC state structure
//! @param[in] pMeta a pointer to the node controller (NC) metadata structure
//! @param[in] instanceId the instance identifier string (i-XXXXXXXX)
//! @param[in] volumeId the volume identifier string (vol-XXXXXXXX)
//! @param[in] remoteDev the target device name
//! @param[in] localDev the local device name
//! @param[in] force if set to 1, this will force the volume to detach
//! @param[in] grab_inst_sem if set to 1, will require the usage of the instance semaphore
//!
//! @return EUCA_OK on success or proper error code. Known error code returned include: EUCA_ERROR,
//!         EUCA_NOT_FOUND_ERROR and EUCA_HYPERVISOR_ERROR.
//!
//! @see convert_dev_names()
//!
static int doDetachVolume(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, char *volumeId, char *remoteDev, char *localDev, int force,
                          int grab_inst_sem)
{
    int ret = EUCA_OK;
    int is_iscsi_target = 0;
    int have_remote_device = 0;
    char *xml = NULL;
    char resourceName[1][MAX_SENSOR_NAME_LEN] = { {0} };
    char resourceAlias[1][MAX_SENSOR_NAME_LEN] = { {0} };

    char *tagBuf;
    char *localDevName;
    char localDevReal[32], localDevTag[256], remoteDevReal[32];
    if (!strcmp(nc->H->name, "xen")) {
        tagBuf = NULL;
        localDevName = localDevReal;
    } else if (!strcmp(nc->H->name, "kvm")) {
        tagBuf = localDevTag;
        localDevName = localDevTag;
    } else {
        logprintfl(EUCAERROR, "[%s][%s] unknown hypervisor type '%s'\n", instanceId, volumeId, nc->H->name);
        return EUCA_ERROR;
    }

    // get the file name from the device path and, for KVM, the "unknown" string
    ret = convert_dev_names(localDev, localDevReal, tagBuf);
    if (ret)
        return ret;

    // find the instance record
    if (grab_inst_sem)
        sem_p(inst_sem);
    ncInstance *instance = find_instance(&global_instances, instanceId);
    if (grab_inst_sem)
        sem_v(inst_sem);
    if (instance == NULL)
        return EUCA_NOT_FOUND_ERROR;

    // try attaching to hypervisor
    virConnectPtr *conn = check_hypervisor_conn();
    if (conn == NULL) {
        logprintfl(EUCAERROR, "[%s][%s] cannot get connection to hypervisor\n", instanceId, volumeId);
        return EUCA_HYPERVISOR_ERROR;
    }
    // find domain on hypervisor
    sem_p(hyp_sem);
    virDomainPtr dom = virDomainLookupByName(*conn, instanceId);
    sem_v(hyp_sem);
    if (dom == NULL) {
        if (instance->state != BOOTING && instance->state != STAGING) {
            logprintfl(EUCAWARN, "[%s][%s] domain not running on hypervisor, cannot attach device\n", instanceId, volumeId);
        }
        return EUCA_HYPERVISOR_ERROR;
    }
    // mark volume as 'detaching'
    ncVolume *volume;
    if (grab_inst_sem)
        sem_p(inst_sem);
    volume = save_volume(instance, volumeId, remoteDev, localDevName, localDevReal, VOL_STATE_DETACHING);
    save_instance_struct(instance);
    copy_instances();
    if (grab_inst_sem)
        sem_v(inst_sem);
    if (!volume) {
        logprintfl(EUCAERROR, "[%s][%s] failed to update the volume record, aborting volume attachment\n", instanceId, volumeId);
        return EUCA_ERROR;
    }
    // do iscsi connect shellout if remoteDev is an iSCSI target
    if (check_iscsi(remoteDev)) {
        char *remoteDevStr = NULL;
        is_iscsi_target = 1;

        // get credentials, decrypt them
        remoteDevStr = get_iscsi_target(remoteDev);
        if (!remoteDevStr || !strstr(remoteDevStr, "/dev")) {
            logprintfl(EUCAERROR, "[%s][%s] failed to get local name of host iscsi device\n", instanceId, volumeId);
            remoteDevReal[0] = '\0';
        } else {
            snprintf(remoteDevReal, 32, "%s", remoteDevStr);
            have_remote_device = 1;
        }
        EUCA_FREE(remoteDevStr);
    } else {
        snprintf(remoteDevReal, 32, "%s", remoteDev);
        have_remote_device = 1;
    }

    // something went wrong above, abort
    if (!have_remote_device) {
        ret = EUCA_ERROR;
        goto release;
    }
    // make sure there is a block device
    if (check_block(remoteDevReal)) {
        logprintfl(EUCAERROR, "[%s][%s] cannot verify that host device '%s' is available for hypervisor detach\n", instanceId, volumeId,
                   remoteDevReal);
        if (!force)
            ret = EUCA_ERROR;
        goto release;
    }

    euca_strncpy(resourceName[0], instance->instanceId, MAX_SENSOR_NAME_LEN);
    sensor_refresh_resources(resourceName, resourceAlias, 1);   // refresh stats so volume measurements are captured before it disappears

    char path[MAX_PATH];
    char lpath[MAX_PATH];
    snprintf(path, sizeof(path), EUCALYPTUS_VOLUME_XML_PATH_FORMAT, instance->instancePath, volumeId);  // vol-XXX.xml
    snprintf(lpath, sizeof(lpath), EUCALYPTUS_VOLUME_LIBVIRT_XML_PATH_FORMAT, instance->instancePath, volumeId);    // vol-XXX-libvirt.xml

    // read in libvirt XML
    xml = file2str(lpath);
    if (xml == NULL) {
        logprintfl(EUCAERROR, "[%s][%s] failed to read volume XML from %s\n", instance->instanceId, volumeId, lpath);
        ret = EUCA_ERROR;
        goto release;
    }
    // protect libvirt calls because we've seen problems during concurrent libvirt invocations
    sem_p(hyp_sem);
    int err = virDomainDetachDevice(dom, xml);
    if (!strcmp(nc->H->name, "xen")) {
        err = xen_detach_helper(nc, instanceId, localDevReal, xml);
    }
    sem_v(hyp_sem);

    if (err) {
        logprintfl(EUCAERROR, "[%s][%s] failed to detach host device '%s' from guest device '%s'\n", instanceId, volumeId, remoteDevReal,
                   localDevReal);
        logprintfl(EUCAERROR, "[%s][%s] virDomainDetachDevice() or 'virsh detach' failed (err=%d) XML='%s'\n", instanceId, volumeId, err, xml);
        if (!force)
            ret = EUCA_HYPERVISOR_ERROR;
    } else {
        call_hooks(NC_EVENT_POST_DETACH, path); // invoke hooks, but do not do anything if they return error
        unlink(lpath);          // remove vol-XXX-libvirt.xml
        unlink(path);           // remove vol-XXXX.xml file
    }

release:
    sem_p(hyp_sem);
    virDomainFree(dom);         // release libvirt resource
    sem_v(hyp_sem);
    // record volume state in memory and on disk
    char *next_vol_state;
    if (ret == EUCA_OK) {
        next_vol_state = VOL_STATE_DETACHED;
    } else {
        next_vol_state = VOL_STATE_DETACHING_FAILED;
    }
    if (grab_inst_sem)
        sem_p(inst_sem);
    volume = save_volume(instance, volumeId, NULL, NULL, NULL, next_vol_state);
    save_instance_struct(instance);
    copy_instances();
    update_disk_aliases(instance);  // ask sensor subsystem to stop tracking the volume
    if (grab_inst_sem)
        sem_v(inst_sem);
    if (volume == NULL) {
        logprintfl(EUCAWARN, "[%s][%s] failed to save the volume record\n", instanceId, volumeId);
        ret = EUCA_ERROR;
    }
    // if iSCSI, try to disconnect the target
    if (is_iscsi_target && have_remote_device) {
        logprintfl(EUCADEBUG, "[%s][%s] attempting to disconnect iscsi target\n", instanceId, volumeId);
        if (disconnect_iscsi_target(remoteDev) != 0) {
            logprintfl(EUCAERROR, "[%s][%s] failed to disconnet iscsi target\n", instanceId, volumeId);
            if (!force)
                ret = EUCA_ERROR;
        }
    }

    if (ret == EUCA_OK)
        logprintfl(EUCAINFO, "[%s][%s] detached as host device '%s' and guest device '%s'\n", instanceId, volumeId, remoteDevReal, localDevReal);
    // remoteDev can be a long string, so to keep log readable, we log it at TRACE level unless there was a problem
    int log_level_for_devstring = EUCATRACE;
    if (ret != EUCA_OK)
        log_level_for_devstring = EUCADEBUG;
    logprintfl(log_level_for_devstring, "[%s][%s] remote device string: %s\n", instanceId, volumeId, remoteDev);

    EUCA_FREE(xml);

    if (force) {
        return (EUCA_OK);
    }
    return ret;
}

//!
//! helper for changing bundling task state and stateName together
//!
//! @param[in] instance a pointer to the instance we are changing the state for
//! @param[in] state the new state
//!
static void change_createImage_state(ncInstance * instance, createImage_progress state)
{
    instance->createImageTaskState = state;
    euca_strncpy(instance->createImageTaskStateName, createImage_progress_names[state], CHAR_BUFFER_SIZE);
}

//!
//! helper for cleaning up
//!
//! @param[in] instance a pointer ot the instance to cleanup
//! @param[in] params a pointer to the create image parameters
//! @param[in] state the new instance to set
//! @param[in] result the result of the create image processing
//!
//! @return EUCA_OK on success otherwise EUCA_ERROR is returned.
//!
static int cleanup_createImage_task(ncInstance * instance, struct createImage_params_t *params, instance_states state, createImage_progress result)
{
    logprintfl(EUCAINFO, "[%s] createImage task result=%s\n", instance->instanceId, createImage_progress_names[result]);
    sem_p(inst_sem);
    {
        change_createImage_state(instance, result);
        if (state != NO_STATE)  // do not touch instance state (these are early failures, before we destroyed the domain)
            change_state(instance, state);
        copy_instances();
    }
    sem_v(inst_sem);

    if (params) {
        // if the result was failed or cancelled, clean up walrus state
        if (result == CREATEIMAGE_FAILED || result == CREATEIMAGE_CANCELLED) {
        }
        EUCA_FREE(params->workPath);
        EUCA_FREE(params->volumeId);
        EUCA_FREE(params->remoteDev);
        EUCA_FREE(params->diskPath);
        EUCA_FREE(params->eucalyptusHomePath);
        EUCA_FREE(params);
    }

    return (result == CREATEIMAGE_SUCCESS) ? EUCA_OK : EUCA_ERROR;
}

//!
//! Defines the create image thread.
//!
//! @param[in] arg a transparent pointer to the argument passed to this thread handler
//!
//! @return Always return NULL
//!
static void *createImage_thread(void *arg)
{
    struct createImage_params_t *params = (struct createImage_params_t *)arg;
    ncInstance *instance = params->instance;
    int rc;

    logprintfl(EUCADEBUG, "[%s] spawning create-image thread\n", instance->instanceId);
    logprintfl(EUCAINFO, "[%s] waiting for instance to shut down\n", instance->instanceId);
    // wait until monitor thread changes the state of the instance instance
    if (wait_state_transition(instance, CREATEIMAGE_SHUTDOWN, CREATEIMAGE_SHUTOFF)) {
        if (instance->createImageCanceled) {    // cancel request came in while the instance was shutting down
            logprintfl(EUCAINFO, "[%s] cancelled while createImage for instance\n", instance->instanceId);
            cleanup_createImage_task(instance, params, SHUTOFF, CREATEIMAGE_CANCELLED);
        } else {
            logprintfl(EUCAINFO, "[%s] failed while createImage for instance\n", instance->instanceId);
            cleanup_createImage_task(instance, params, SHUTOFF, CREATEIMAGE_FAILED);
        }
        return NULL;
    }

    logprintfl(EUCAINFO, "[%s] started createImage for instance\n", instance->instanceId);
    {
        rc = 0;
        if (rc == 0) {
            cleanup_createImage_task(instance, params, SHUTOFF, CREATEIMAGE_SUCCESS);
            logprintfl(EUCAINFO, "[%s] finished createImage for instance\n", instance->instanceId);
        } else if (rc == -1) {
            // bundler child was cancelled (killed)
            cleanup_createImage_task(instance, params, SHUTOFF, CREATEIMAGE_CANCELLED);
            logprintfl(EUCAINFO, "[%s] cancelled while createImage for instance (rc=%d)\n", instance->instanceId, rc);
        } else {
            cleanup_createImage_task(instance, params, SHUTOFF, CREATEIMAGE_FAILED);
            logprintfl(EUCAINFO, "[%s] failed while createImage for instance (rc=%d)\n", instance->instanceId, rc);
        }
    }

    return NULL;
}

//!
//! Handles the image creation request.
//!
//! @param[in] nc a pointer to the NC state structure
//! @param[in] pMeta a pointer to the node controller (NC) metadata structure
//! @param[in] instanceId the instance identifier string (i-XXXXXXXX)
//! @param[in] volumeId the volume identifier string (vol-XXXXXXXX)
//! @param[in] remoteDev the remote device name
//!
//! @return EUCA_OK on success or proper error code. Known error code returned include: EUCA_ERROR,
//!         EUCA_NOT_FOUND_ERROR. Error code from cleanup_createImage_task() and find_and_terminate_instance()
//!         are also returned.
//!
//! @see cleanup_createImage_task()
//! @see find_and_terminate_instance()
//!
static int doCreateImage(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, char *volumeId, char *remoteDev)
{
    // sanity checking
    if (instanceId == NULL || remoteDev == NULL || volumeId == NULL) {
        logprintfl(EUCAERROR, "[%s][%s] called with invalid parameters\n", ((instanceId == NULL) ? "UNKNOWN" : instanceId),
                   ((volumeId == NULL) ? "UNKNOWN" : volumeId));
        return EUCA_ERROR;
    }
    // find the instance
    ncInstance *instance = find_instance(&global_instances, instanceId);
    if (instance == NULL) {
        logprintfl(EUCAERROR, "[%s][%s] instance not found\n", instanceId, volumeId);
        return EUCA_NOT_FOUND_ERROR;
    }
    // "marshall" thread parameters
    struct createImage_params_t *params = EUCA_ZALLOC(1, sizeof(struct createImage_params_t));
    if (params == NULL)
        return cleanup_createImage_task(instance, params, NO_STATE, CREATEIMAGE_FAILED);

    params->instance = instance;
    params->volumeId = strdup(volumeId);
    params->remoteDev = strdup(remoteDev);

    // terminate the instance
    sem_p(inst_sem);
    instance->createImageTime = time(NULL);
    change_state(instance, CREATEIMAGE_SHUTDOWN);
    change_createImage_state(instance, CREATEIMAGE_IN_PROGRESS);

    int err = find_and_terminate_instance(nc, pMeta, instanceId, 0, &instance, 1);
    if (err != EUCA_OK) {
        copy_instances();
        sem_v(inst_sem);
        EUCA_FREE(params);
        return err;
    }
    copy_instances();
    sem_v(inst_sem);

    // do the rest in a thread
    pthread_attr_t tattr;
    pthread_t tid;
    pthread_attr_init(&tattr);
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&tid, &tattr, createImage_thread, (void *)params) != 0) {
        logprintfl(EUCAERROR, "[%s][%s] failed to start VM createImage thread\n", instanceId, volumeId);
        return cleanup_createImage_task(instance, params, SHUTOFF, CREATEIMAGE_FAILED);
    }

    return EUCA_OK;
}

//!
//! helper for changing bundling task state and stateName together
//!
//! @param[in] instance a pointer to the instance we are changing the bundling state for
//! @param[in] state the new bundling state
//!
static void change_bundling_state(ncInstance * instance, bundling_progress state)
{
    instance->bundleTaskState = state;
    euca_strncpy(instance->bundleTaskStateName, bundling_progress_names[state], CHAR_BUFFER_SIZE);
}

//!
//! API to restart an instance. This will prepare the restart thread and start it.
//!
//! @param[in] instance a pointer to the instance to restart
//!
//! @return EUCA_OK on success or proper error code. Known error code returned include: EUCA_ERROR,
//!         EUCA_MEMORY_ERROR, EUCA_THREAD_ERROR.
//!
static int restart_instance(ncInstance * instance)
{
    int error = EUCA_ERROR;
    pthread_attr_t *attr = NULL;

    // Reset a few fields to prevent future confusion
    instance->state = STAGING;
    instance->retries = LIBVIRT_QUERY_RETRIES;
    instance->launchTime = time(NULL);
    //instance->expiryTime           = xxx?
    instance->bootTime = 0;
    instance->bundlingTime = 0;
    instance->createImageTime = 0;
    instance->terminationTime = 0;
    instance->bundlePid = 0;
    instance->bundleCanceled = 0;
    instance->bundleBucketExists = 0;
    instance->stateCode = EXTANT;
    instance->bundleTaskState = NOT_BUNDLING;
    instance->createImageTaskState = NOT_CREATEIMAGE;
    instance->createImagePid = 0;
    instance->createImageCanceled = 0;

    euca_strncpy(instance->stateName, instance_state_names[EXTANT], CHAR_BUFFER_SIZE);
    euca_strncpy(instance->bundleTaskStateName, bundling_progress_names[NOT_BUNDLING], CHAR_BUFFER_SIZE);
    euca_strncpy(instance->createImageTaskStateName, createImage_progress_names[NOT_CREATEIMAGE], CHAR_BUFFER_SIZE);

    // Reset our pthread structure
    memset(&(instance->tcb), 0, sizeof(instance->tcb));

    // to enable NC recovery
    save_instance_struct(instance);

    // do the potentially long tasks in a thread
    if ((attr = (pthread_attr_t *) EUCA_ZALLOC(1, sizeof(pthread_attr_t))) == NULL) {
        logprintfl(EUCAERROR, "[%s] out of memory\n", instance->instanceId);
        error = EUCA_MEMORY_ERROR;
        goto error;
    }

    pthread_attr_init(attr);
    pthread_attr_setdetachstate(attr, PTHREAD_CREATE_DETACHED);

    if (pthread_create(&(instance->tcb), attr, restart_thread, ((void *)instance))) {
        pthread_attr_destroy(attr);
        logprintfl(EUCAERROR, "[%s] failed to spawn a VM startup thread\n", instance->instanceId);
        error = EUCA_THREAD_ERROR;
        sem_p(inst_sem);
        {
            remove_instance(&global_instances, instance);
            copy_instances();
        }
        sem_v(inst_sem);

        EUCA_FREE(attr);
        goto error;
    }

    pthread_attr_destroy(attr);
    EUCA_FREE(attr);
    return (EUCA_OK);

error:
    free_instance(&instance);
    return (error);
}

//!
//! Cleans up the bundling task uppon completion or cancellation.
//!
//! @param[in] instance a pointer to the instance we're cleaning the bundling task for
//! @param[in] params a pointer to the bundling task parameters
//! @param[in] result the bundling task result
//!
//! @return EUCA_OK on success or EUCA_ERROR on failure.
//!
static int cleanup_bundling_task(ncInstance * instance, struct bundling_params_t *params, bundling_progress result)
{
    int rc = 0;
    char cmd[MAX_PATH] = { 0 };
    char buf[MAX_PATH] = { 0 };

    logprintfl(EUCAINFO, "[%s] bundling task result=%s\n", instance->instanceId, bundling_progress_names[result]);

    sem_p(inst_sem);
    {
        change_bundling_state(instance, result);
        copy_instances();
    }
    sem_v(inst_sem);

    if (params) {
        // if the result was failed or cancelled, clean up walrus state
        if ((result == BUNDLING_FAILED) || (result == BUNDLING_CANCELLED)) {
            if (!instance->bundleBucketExists) {
                snprintf(cmd, MAX_PATH, "%s -b %s -p %s --euca-auth", params->ncDeleteBundleCmd, params->bucketName, params->filePrefix);
            } else {
                snprintf(cmd, MAX_PATH, "%s -b %s -p %s --euca-auth --clear", params->ncDeleteBundleCmd, params->bucketName, params->filePrefix);
                instance->bundleBucketExists = 0;
            }

            // set up environment for euca2ools
            snprintf(buf, MAX_PATH, EUCALYPTUS_KEYS_DIR "/node-cert.pem", params->eucalyptusHomePath);
            setenv("EC2_CERT", buf, 1);

            snprintf(buf, MAX_PATH, "IGNORED");
            setenv("EC2_SECRET_KEY", buf, 1);

            snprintf(buf, MAX_PATH, EUCALYPTUS_KEYS_DIR "/cloud-cert.pem", params->eucalyptusHomePath);
            setenv("EUCALYPTUS_CERT", buf, 1);

            snprintf(buf, MAX_PATH, "%s", params->walrusURL);
            setenv("S3_URL", buf, 1);

            snprintf(buf, MAX_PATH, "%s", params->userPublicKey);
            setenv("EC2_ACCESS_KEY", buf, 1);

            snprintf(buf, MAX_PATH, "123456789012");
            setenv("EC2_USER_ID", buf, 1);

            snprintf(buf, MAX_PATH, EUCALYPTUS_KEYS_DIR "/node-cert.pem", params->eucalyptusHomePath);
            setenv("EUCA_CERT", buf, 1);

            snprintf(buf, MAX_PATH, EUCALYPTUS_KEYS_DIR "/node-pk.pem", params->eucalyptusHomePath);
            setenv("EUCA_PRIVATE_KEY", buf, 1);

            logprintfl(EUCADEBUG, "running cmd '%s'\n", cmd);
            rc = system(cmd);
            rc = rc >> 8;
            if (rc) {
                logprintfl(EUCAWARN, "[%s] bucket cleanup cmd '%s' failed with rc '%d'\n", instance->instanceId, cmd, rc);
            }
        }

        EUCA_FREE(params->workPath);
        EUCA_FREE(params->bucketName);
        EUCA_FREE(params->filePrefix);
        EUCA_FREE(params->walrusURL);
        EUCA_FREE(params->userPublicKey);
        EUCA_FREE(params->diskPath);
        EUCA_FREE(params->eucalyptusHomePath);
        EUCA_FREE(params->ncBundleUploadCmd);
        EUCA_FREE(params->ncCheckBucketCmd);
        EUCA_FREE(params->ncDeleteBundleCmd);
        EUCA_FREE(params);
    }

    return ((result == BUNDLING_SUCCESS) ? EUCA_OK : EUCA_ERROR);
}

//!
//! Defines the bundling thread
//!
//! @param[in] arg a transparent pointer to the argument passed to this thread handler
//!
//! @return Always return NULL
//!
static void *bundling_thread(void *arg)
{
    struct bundling_params_t *params = (struct bundling_params_t *)arg;
    ncInstance *instance = params->instance;
    char cmd[MAX_PATH];
    char buf[MAX_PATH];

    logprintfl(EUCADEBUG, "[%s] spawning bundling thread\n", instance->instanceId);
    logprintfl(EUCAINFO, "[%s] waiting for instance to shut down\n", instance->instanceId);
    // wait until monitor thread changes the state of the instance instance
    if (wait_state_transition(instance, BUNDLING_SHUTDOWN, BUNDLING_SHUTOFF)) {
        if (instance->bundleCanceled) { // cancel request came in while the instance was shutting down
            logprintfl(EUCAINFO, "[%s] cancelled while bundling instance\n", instance->instanceId);
            cleanup_bundling_task(instance, params, BUNDLING_CANCELLED);
        } else {
            logprintfl(EUCAINFO, "[%s] failed while bundling instance\n", instance->instanceId);
            cleanup_bundling_task(instance, params, BUNDLING_FAILED);
        }
        return NULL;
    }

    logprintfl(EUCAINFO, "[%s] started bundling instance\n", instance->instanceId);

    int rc = EUCA_OK;
    char bundlePath[MAX_PATH];
    bundlePath[0] = '\0';
    if (clone_bundling_backing(instance, params->filePrefix, bundlePath) != EUCA_OK) {
        logprintfl(EUCAERROR, "[%s] could not clone the instance image\n", instance->instanceId);
        cleanup_bundling_task(instance, params, BUNDLING_FAILED);
    } else {
        char prefixPath[MAX_PATH];
        snprintf(prefixPath, MAX_PATH, "%s/%s", instance->instancePath, params->filePrefix);
        if (strcmp(bundlePath, prefixPath) != 0 && rename(bundlePath, prefixPath) != 0) {
            logprintfl(EUCAERROR, "[%s] could not rename from %s to %s\n", instance->instanceId, bundlePath, prefixPath);
            cleanup_bundling_task(instance, params, BUNDLING_FAILED);
            return NULL;
        }
        // USAGE: euca-nc-bundle-upload -i <image_path> -d <working dir> -b <bucket>
        int pid, status;

        // set up environment for euca2ools
        snprintf(buf, MAX_PATH, EUCALYPTUS_KEYS_DIR "/node-cert.pem", params->eucalyptusHomePath);
        setenv("EC2_CERT", buf, 1);

        snprintf(buf, MAX_PATH, "IGNORED");
        setenv("EC2_SECRET_KEY", buf, 1);

        snprintf(buf, MAX_PATH, EUCALYPTUS_KEYS_DIR "/cloud-cert.pem", params->eucalyptusHomePath);
        setenv("EUCALYPTUS_CERT", buf, 1);

        snprintf(buf, MAX_PATH, "%s", params->walrusURL);
        setenv("S3_URL", buf, 1);

        snprintf(buf, MAX_PATH, "%s", params->userPublicKey);
        setenv("EC2_ACCESS_KEY", buf, 1);

        snprintf(buf, MAX_PATH, "123456789012");
        setenv("EC2_USER_ID", buf, 1);

        snprintf(buf, MAX_PATH, EUCALYPTUS_KEYS_DIR "/node-cert.pem", params->eucalyptusHomePath);
        setenv("EUCA_CERT", buf, 1);

        snprintf(buf, MAX_PATH, EUCALYPTUS_KEYS_DIR "/node-pk.pem", params->eucalyptusHomePath);
        setenv("EUCA_PRIVATE_KEY", buf, 1);

        // check to see if the bucket exists in advance
        snprintf(cmd, MAX_PATH, "%s -b %s --euca-auth", params->ncCheckBucketCmd, params->bucketName);
        logprintfl(EUCADEBUG, "[%s] running cmd '%s'\n", instance->instanceId, cmd);
        rc = system(cmd);
        rc = rc >> 8;
        instance->bundleBucketExists = rc;

        if (instance->bundleCanceled) {
            logprintfl(EUCAINFO, "[%s] bundle task canceled; terminating bundling thread\n", instance->instanceId);
            cleanup_bundling_task(instance, params, BUNDLING_CANCELLED);
            return NULL;
        }

        pid = fork();
        if (!pid) {
            logprintfl(EUCADEBUG, "[%s] running cmd '%s -i %s -d %s -b %s -c %s --policysignature %s --euca-auth'\n", instance->instanceId,
                       params->ncBundleUploadCmd, prefixPath, params->workPath, params->bucketName, params->S3Policy, params->S3PolicySig);
            exit(execlp
                 (params->ncBundleUploadCmd, params->ncBundleUploadCmd, "-i", prefixPath, "-d", params->workPath, "-b", params->bucketName, "-c",
                  params->S3Policy, "--policysignature", params->S3PolicySig, "--euca-auth", NULL));
        } else {
            instance->bundlePid = pid;
            rc = waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                rc = WEXITSTATUS(status);
            } else {
                rc = -1;
            }
        }

        if (rc == 0) {
            cleanup_bundling_task(instance, params, BUNDLING_SUCCESS);
            logprintfl(EUCAINFO, "[%s] finished bundling instance\n", instance->instanceId);
        } else if (rc == -1) {
            // bundler child was cancelled (killed), but should report it as failed
            cleanup_bundling_task(instance, params, BUNDLING_FAILED);
            logprintfl(EUCAINFO, "[%s] cancelled while bundling instance (rc=%d)\n", instance->instanceId, rc);
        } else {
            cleanup_bundling_task(instance, params, BUNDLING_FAILED);
            logprintfl(EUCAINFO, "[%s] failed while bundling instance (rc=%d)\n", instance->instanceId, rc);
        }
    }
    return NULL;
}

//!
//! Handles the bundling instance request.
//!
//! @param[in] nc a pointer to the NC state structure
//! @param[in] pMeta a pointer to the node controller (NC) metadata structure
//! @param[in] instanceId the instance identifier string (i-XXXXXXXX)
//! @param[in] bucketName the bucket name string to which the bundle will be saved
//! @param[in] filePrefix the prefix name string of the bundle
//! @param[in] walrusURL the walrus URL address string
//! @param[in] userPublicKey the public key string
//! @param[in] S3Policy the S3 engine policy
//! @param[in] S3PolicySig the S3 engine policy signature
//!
//! @return EUCA_OK on success or proper error code. known error code returned include: EUCA_ERROR and
//!         EUCA_NOT_FOUND_ERROR. Error code from cleanup_bundling_task() and find_and_terminate_instance()
//!         are also returned.
//!
//! @see cleanup_bundling_task()
//! @see find_and_terminate_instance()
//!
static int doBundleInstance(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId, char *bucketName, char *filePrefix, char *walrusURL,
                            char *userPublicKey, char *S3Policy, char *S3PolicySig)
{
    // sanity checking
    if (instanceId == NULL || bucketName == NULL || filePrefix == NULL || walrusURL == NULL || userPublicKey == NULL || S3Policy == NULL
        || S3PolicySig == NULL) {
        logprintfl(EUCAERROR, "[%s] bundling instance called with invalid parameters\n", ((instanceId == NULL) ? "UNKNOWN" : instanceId));
        return EUCA_ERROR;
    }
    // find the instance
    ncInstance *instance = find_instance(&global_instances, instanceId);
    if (instance == NULL) {
        logprintfl(EUCAERROR, "[%s] instance not found\n", instanceId);
        return EUCA_NOT_FOUND_ERROR;
    }
    // "marshall" thread parameters
    struct bundling_params_t *params = EUCA_ZALLOC(1, sizeof(struct bundling_params_t));
    if (params == NULL)
        return cleanup_bundling_task(instance, params, BUNDLING_FAILED);

    params->instance = instance;
    params->bucketName = strdup(bucketName);
    params->filePrefix = strdup(filePrefix);
    params->walrusURL = strdup(walrusURL);
    params->userPublicKey = strdup(userPublicKey);
    params->S3Policy = strdup(S3Policy);
    params->S3PolicySig = strdup(S3PolicySig);
    params->eucalyptusHomePath = strdup(nc->home);
    params->ncBundleUploadCmd = strdup(nc->ncBundleUploadCmd);
    params->ncCheckBucketCmd = strdup(nc->ncCheckBucketCmd);
    params->ncDeleteBundleCmd = strdup(nc->ncDeleteBundleCmd);

    params->workPath = strdup(instance->instancePath);

    // terminate the instance
    sem_p(inst_sem);
    instance->bundlingTime = time(NULL);
    change_state(instance, BUNDLING_SHUTDOWN);
    change_bundling_state(instance, BUNDLING_IN_PROGRESS);

    int err = find_and_terminate_instance(nc, pMeta, instanceId, 0, &instance, 1);
    copy_instances();
    sem_v(inst_sem);

    if (err != EUCA_OK) {
        EUCA_FREE(params);
        return err;
    }
    // do the rest in a thread
    pthread_attr_t tattr;
    pthread_t tid;
    pthread_attr_init(&tattr);
    pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&tid, &tattr, bundling_thread, (void *)params) != 0) {
        logprintfl(EUCAERROR, "[%s] failed to start VM bundling thread\n", instanceId);
        return cleanup_bundling_task(instance, params, BUNDLING_FAILED);
    }

    return EUCA_OK;
}

//!
//! Handles the bundle restart request.
//!
//! @param[in] nc a pointer to the NC state structure
//! @param[in] pMeta a pointer to the node controller (NC) metadata structure
//! @param[in] instanceId the instance identifier string (i-XXXXXXXX)
//!
//! @return the result of the restart_instance() call, EUCA_ERROR or EUCA_NOT_FOUND_ERROR.
//!
//! @see restart_instance()
//!
static int doBundleRestartInstance(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId)
{
    ncInstance *instance = NULL;

    // sanity checking
    if (instanceId == NULL) {
        logprintfl(EUCAERROR, "bundle restart instance called with invalid parameters\n");
        return (EUCA_ERROR);
    }
    // find the instance
    if ((instance = find_instance(&global_instances, instanceId)) == NULL) {
        logprintfl(EUCAERROR, "[%s] instance not found\n", instanceId);
        return (EUCA_NOT_FOUND_ERROR);
    }
    // Now restart this instance regardless of bundling success or failure
    return (restart_instance(instance));
}

//!
//! Handles the cancel bundle task request.
//!
//! @param[in] nc a pointer to the NC state structure
//! @param[in] pMeta a pointer to the node controller (NC) metadata structure
//! @param[in] instanceId the instance identifier string (i-XXXXXXXX)
//!
//! @return EUCA_OK on success or EUCA_NOT_FOUND_ERROR on failure.
//!
static int doCancelBundleTask(struct nc_state_t *nc, ncMetadata * pMeta, char *instanceId)
{
    ncInstance *instance = find_instance(&global_instances, instanceId);
    if (instance == NULL) {
        logprintfl(EUCAERROR, "[%s] instance not found\n", instanceId);
        return EUCA_NOT_FOUND_ERROR;
    }

    instance->bundleCanceled = 1;   // record the intent to cancel bundling so that bundling thread can abort
    if ((instance->bundlePid > 0) && !check_process(instance->bundlePid, "euca-bundle-upload")) {
        logprintfl(EUCADEBUG, "[%s] found bundlePid '%d', sending kill signal...\n", instanceId, instance->bundlePid);
        kill(instance->bundlePid, 9);
        instance->bundlePid = 0;
    }

    return (EUCA_OK);
}

//!
//! Handles the describe bundle tasks request.
//!
//! @param[in]  nc a pointer to the NC state structure
//! @param[in]  pMeta a pointer to the node controller (NC) metadata structure
//! @param[in]  instIds a list of instance identifier string
//! @param[in]  instIdsLen the number of instance identifiers in the instIds list
//! @param[out] outBundleTasks a pointer to the created bundle tasks list
//! @param[out] outBundleTasksLen the number of bundle tasks in the outBundleTasks list
//!
//! @return EUCA_OK on success or proper error code. Known error code returned include: EUCA_ERROR
//!         and EUCA_MEMORY_ERROR.
//!
static int doDescribeBundleTasks(struct nc_state_t *nc, ncMetadata * pMeta, char **instIds, int instIdsLen, bundleTask *** outBundleTasks,
                                 int *outBundleTasksLen)
{
    if (instIdsLen < 1 || instIds == NULL) {
        logprintfl(EUCADEBUG, "internal error (invalid parameters to doDescribeBundleTasks)\n");
        return EUCA_ERROR;
    }
    // Maximum size
    *outBundleTasks = EUCA_ZALLOC(instIdsLen, sizeof(bundleTask *));
    if ((*outBundleTasks) == NULL) {
        return EUCA_MEMORY_ERROR;
    }

    *outBundleTasksLen = 0;     // we may return fewer than instIdsLen

    int i, j;
    for (i = 0, j = 0; i < instIdsLen; i++) {
        bundleTask *bundle = NULL;

        sem_p(inst_sem);
        ncInstance *instance = find_instance(&global_instances, instIds[i]);
        if (instance != NULL) {
            bundle = EUCA_ZALLOC(1, sizeof(bundleTask));
            if (bundle == NULL) {
                logprintfl(EUCAERROR, "out of memory\n");
                sem_v(inst_sem);
                return EUCA_MEMORY_ERROR;
            }
            allocate_bundleTask(bundle, instIds[i], instance->bundleTaskStateName);
        }
        sem_v(inst_sem);

        if (bundle) {
            (*outBundleTasks)[j++] = bundle;
            (*outBundleTasksLen)++;
        }
    }

    return EUCA_OK;
}

//!
//! Handles the describe sensors request.
//!
//! @param[in]  nc a pointer to the NC state structure
//! @param[in]  pMeta a pointer to the node controller (NC) metadata structure
//! @param[in]  historySize teh size of the data history to retrieve
//! @param[in]  collectionIntervalTimeMs the data collection interval in milliseconds
//! @param[in]  instIds the list of instance identifiers string
//! @param[in]  instIdsLen the number of instance identifiers in the instIds list
//! @param[in]  sensorIds a list of sensor identifiers string
//! @param[in]  sensorIdsLen the number of sensor identifiers string in the sensorIds list
//! @param[out] outResources a list of sensor resources created by this request
//! @param[out] outResourcesLen the number of sensor resources contained in the outResources list
//!
//! @return EUCA_OK on success or EUCA_MEMORY_ERROR on failure.
//!
static int doDescribeSensors(struct nc_state_t *nc, ncMetadata * pMeta, int historySize, long long collectionIntervalTimeMs, char **instIds,
                             int instIdsLen, char **sensorIds, int sensorIdsLen, sensorResource *** outResources, int *outResourcesLen)
{
    int total;

    int err = sensor_config(historySize, collectionIntervalTimeMs); // update the config parameters if they are different
    if (err != 0)
        logprintfl(EUCAERROR, "failed to update sensor configuration (err=%d)\n", err);

    sem_p(inst_copy_sem);
    if (instIdsLen == 0)        // describe all instances
        total = total_instances(&global_instances_copy);
    else
        total = instIdsLen;

    sensorResource **rss = NULL;
    if (total > 0) {
        rss = EUCA_ZALLOC(total, sizeof(sensorResource *));
        if (rss == NULL) {
            sem_v(inst_copy_sem);
            return EUCA_MEMORY_ERROR;
        }
    }

    int k = 0;

    ncInstance *instance;
    for (int i = 0; (instance = get_instance(&global_instances_copy)) != NULL; i++) {
        // only pick ones the user (or admin) is allowed to see
        if (strcmp(pMeta->userId, nc->admin_user_id)
            && strcmp(pMeta->userId, instance->userId))
            continue;

        if (instIdsLen > 0) {
            int j;

            for (j = 0; j < instIdsLen; j++)
                if (!strcmp(instance->instanceId, instIds[j]))
                    break;

            if (j >= instIdsLen)
                // instance of no relevance right now
                continue;
        }

        assert(k < total);
        rss[k] = EUCA_ZALLOC(1, sizeof(sensorResource));
        if (sensor_get_instance_data(instance->instanceId, sensorIds, sensorIdsLen, rss + k, 1) != EUCA_OK) {
            logprintfl(EUCADEBUG, "[%s] failed to retrieve sensor data\n", instance->instanceId);
            EUCA_FREE(rss[k]);
        } else {
            k++;
        }
    }

    *outResourcesLen = k;
    *outResources = rss;
    sem_v(inst_copy_sem);

    logprintfl(EUCADEBUG, "found %d resource(s)\n", k);
    return EUCA_OK;
}
