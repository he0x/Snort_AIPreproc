/*
 * =====================================================================================
 *
 *       Filename:  neural_cluster.c
 *
 *    Description:  Perform the clusterization over the output layer of the SOM neural
 *                  network, in order to attempt to find the alerts belonging to the
 *                  same attack scenario. The clusterization is operated through k-means
 *                  using Schwarz criterion in order to find the optimal number of
 *                  clusters, the implementation is in fkmeans/
 *
 *        Version:  0.1
 *        Created:  19/11/2010 18:37:35
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  BlackLight (http://0x00.ath.cx), <blacklight@autistici.org>
 *        Licence:  GNU GPL v.3
 *        Company:  DO WHAT YOU WANT CAUSE A PIRATE IS FREE, YOU ARE A PIRATE!
 *
 * =====================================================================================
 */

#include	"spp_ai.h"

/** \defgroup neural_cluster Module for clustering the alerts associated to the
 * neural network output layer in order to find alerts belonging to the same scenario
 * @{ */

#include	"fkmeans/kmeans.h"

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<sys/stat.h>
#include	<unistd.h>
#include	<time.h>

/**
 * \brief  Print the clusters associated to the SOM output to an XML log file
 * \param  km 				k-means object
 * \param  alerts_per_neuron 	Hash table containing the alerts associated to each neuron
 */

PRIVATE void
__AI_neural_clusters_to_xml ( kmeans_t *km, AI_alerts_per_neuron *alerts_per_neuron )
{
	int i, j, k, l, m, n, are_equal;
	FILE *fp = NULL;

	uint32_t src_addr = 0,
		    dst_addr = 0;

	char src_ip[INET_ADDRSTRLEN] = { 0 },
		dst_ip[INET_ADDRSTRLEN] = { 0 },
		*timestamp = NULL,
		*tmp = NULL,
		*buf = NULL;

	AI_alerts_per_neuron_key key, tmp_key;
	AI_alerts_per_neuron *alert_iterator = NULL,
					 *tmp_iterator = NULL;

	if ( !( fp = fopen ( config->neural_clusters_log, "w" )))
	{
		AI_fatal_err ( "Unable to write on the neural clusters XML log file", __FILE__, __LINE__ );
	}

	fprintf ( fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<?xml-stylesheet href=\"default.xsl\" type=\"text/xsl\"?>\n"
		"<!DOCTYPE neural-clusters PUBLIC \"-//blacklight//DTD NEURAL CLUSTERS//EN\" "
		"\"http://0x00.ath.cx/neural_clusters.dtd\">\n\n"
		"<clusters>\n" );

	for ( i=0; i < km->k; i++ )
	{
		fprintf ( fp, "\t<cluster id=\"%d\">\n", i );

		for ( j=0; j < km->cluster_sizes[i]; j++ )
		{
			key.x = km->clusters[i][j][0];
			key.y = km->clusters[i][j][1];
			HASH_FIND ( hh, alerts_per_neuron, &key, sizeof ( key ), alert_iterator );

			if ( alert_iterator )
			{
				for ( k=0; k < alert_iterator->n_alerts; k++ )
				{
					are_equal = 0;

					for ( l=0; l < alert_iterator->n_alerts && !are_equal; l++ )
					{
						if ( k != l )
						{
							if (
								alert_iterator->alerts[k].gid == alert_iterator->alerts[l].gid &&
								alert_iterator->alerts[k].sid == alert_iterator->alerts[l].sid &&
								alert_iterator->alerts[k].rev == alert_iterator->alerts[l].rev &&
								alert_iterator->alerts[k].src_ip_addr == alert_iterator->alerts[l].src_ip_addr &&
								alert_iterator->alerts[k].dst_ip_addr == alert_iterator->alerts[l].dst_ip_addr &&
								alert_iterator->alerts[k].src_port == alert_iterator->alerts[l].src_port &&
								alert_iterator->alerts[k].dst_port == alert_iterator->alerts[l].dst_port &&
								alert_iterator->alerts[k].timestamp == alert_iterator->alerts[l].timestamp )
							{
								are_equal = 1;
							}
						}
					}

					/* If no duplicate alert was found on the same neuron, check
					 * that there is no duplicate alert on other neurons */
					if ( !are_equal )
					{
						for ( l=0; l < km->k && !are_equal; l++ )
						{
							for ( m=0; m < km->cluster_sizes[l] && !are_equal; m++ )
							{
								if ( l <= i && m < j )
								{
									tmp_key.x = km->clusters[l][m][0];
									tmp_key.y = km->clusters[l][m][1];
									HASH_FIND ( hh, alerts_per_neuron, &tmp_key, sizeof ( tmp_key ), tmp_iterator );

									if ( tmp_iterator )
									{
										for ( n=0; n < tmp_iterator->n_alerts && !are_equal; n++ )
										{
											if (
													alert_iterator->alerts[k].gid == tmp_iterator->alerts[n].gid &&
													alert_iterator->alerts[k].sid == tmp_iterator->alerts[n].sid &&
													alert_iterator->alerts[k].rev == tmp_iterator->alerts[n].rev &&
													alert_iterator->alerts[k].src_ip_addr == tmp_iterator->alerts[n].src_ip_addr &&
													alert_iterator->alerts[k].dst_ip_addr == tmp_iterator->alerts[n].dst_ip_addr &&
													alert_iterator->alerts[k].src_port == tmp_iterator->alerts[n].src_port &&
													alert_iterator->alerts[k].dst_port == tmp_iterator->alerts[n].dst_port &&
													alert_iterator->alerts[k].timestamp == tmp_iterator->alerts[n].timestamp )
											{
												are_equal = 1;
											}
										}
									}
								}
							}
						}
					}

					if ( !are_equal )
					{
						src_addr = htonl ( alert_iterator->alerts[k].src_ip_addr );
						dst_addr = htonl ( alert_iterator->alerts[k].dst_ip_addr );
						inet_ntop ( AF_INET, &src_addr, src_ip, INET_ADDRSTRLEN );
						inet_ntop ( AF_INET, &dst_addr, dst_ip, INET_ADDRSTRLEN );

						timestamp = ctime ( &( alert_iterator->alerts[k].timestamp ));
						timestamp[ strlen ( timestamp ) - 1 ] = 0;

						tmp = str_replace ( alert_iterator->alerts[k].desc, "<", "&lt;" );
						buf = str_replace ( tmp, ">", "&gt;" );
						free ( tmp );
						tmp = NULL;

						fprintf ( fp, "\t\t<alert desc=\"%s\" gid=\"%d\" sid=\"%d\" rev=\"%d\" src_ip=\"%s\" src_port=\"%d\" "
							"dst_ip=\"%s\" dst_port=\"%d\" timestamp=\"%s\" xcoord=\"%d\" ycoord=\"%d\"/>\n",
							buf,
							alert_iterator->alerts[k].gid,
							alert_iterator->alerts[k].sid,
							alert_iterator->alerts[k].rev,
							src_ip, alert_iterator->alerts[k].src_port,
							dst_ip, alert_iterator->alerts[k].dst_port,
							timestamp,
							alert_iterator->key.x, alert_iterator->key.y );

						free ( buf );
						buf = NULL;
					}
				}
			}
		}

		fprintf ( fp, "\t</cluster>\n" );
	}

	fprintf ( fp, "</clusters>\n" );
	fclose ( fp );

	chmod ( config->neural_clusters_log, 0644 );
}		/* -----  end of function __AI_neural_clusters_to_xml  ----- */

/**
 * \brief  Thread that performs the k-means clustering over the output layer of
 * the SOM neural network
 */

void*
AI_neural_clustering_thread ( void *arg )
{
	AI_alerts_per_neuron *alerts_per_neuron = NULL,
					 *alert_iterator    = NULL;

	kmeans_t *km = NULL;
	double **dataset = NULL;
	int i, dataset_size = 0;

	while ( 1 )
	{
		dataset = NULL;
		dataset_size = 0;
		alerts_per_neuron = AI_get_alerts_per_neuron();
		
		for ( alert_iterator = alerts_per_neuron; alert_iterator; alert_iterator = (AI_alerts_per_neuron*) alert_iterator->hh.next )
		{
			if ( alert_iterator->n_alerts > 0 )
			{
				if ( !( dataset = (double**) realloc ( dataset, (++dataset_size) * sizeof ( double* ))))
				{
					AI_fatal_err ( "Fatal dynamic memory allocation error", __FILE__, __LINE__ );
				}

				if ( !( dataset[dataset_size-1] = (double*) calloc ( 2, sizeof ( double ))))
				{
					AI_fatal_err ( "Fatal dynamic memory allocation error", __FILE__, __LINE__ );
				}

				dataset[dataset_size-1][0] = (double) alert_iterator->key.x;
				dataset[dataset_size-1][1] = (double) alert_iterator->key.y;
			}
		}

		if ( dataset && dataset_size != 0 )
		{
			if ( !( km = kmeans_auto ( dataset, dataset_size, 2 )))
			{
				AI_fatal_err ( "Unable to initialize the k-means clustering object", __FILE__, __LINE__ );
			}

			__AI_neural_clusters_to_xml ( km, alerts_per_neuron );
			kmeans_free ( km );

			for ( i=0; i < dataset_size; i++ )
			{
				free ( dataset[i] );
			}

			free ( dataset );
		}

		sleep ( config->neuralClusteringInterval );
	}

	pthread_exit ((void*) 0);
	return (void*) 0;
}		/* -----  end of function AI_neural_clustering_thread  ----- */

/** @} */

