//
// Created by netbook on 29/03/2020.
//
#include "biblio.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "math.h"
#include <netdb.h>



/// ---------------------------------------- fonctions Tlv --------------------------------

int32_t add_tlv( tlv_chain *a, unsigned char type, int16_t size, const unsigned char *bytes)
{
    if(a == NULL )
        return -1;

    // all elements used in chain?
    if(a->used == MAX_TLV_OBJECTS)
        return -1;

    int index = a->used;
    a->object[index].type = type;
    a->object[index].size = size;
    if(type!=0 && type!=5 && type!=2) {
        a->object[index].data = malloc(size);
        memcpy(a->object[index].data, bytes, size);
    } else{ a->object[index].data =NULL;}

    // increase number of tlv objects used in this chain
    a->used++;

    // success
    return 0;
}


// serialize the tlv chain into byte array
int16_t tlv_chain_toBuff( tlv_chain *a, unsigned char *dest, int16_t *count)
{
    if(a == NULL || dest == NULL)
        return -1;

    // Number of bytes serialized
    int16_t counter = 0;
    short lBe;
char len[2];
    for(int i = 0; i < a->used; i++)
    {
        dest[counter] = a->object[i].type;
        counter++;
       if(a->object[i].type!=0) {
          
           lBe=htons(a->object[i].size);
           memcpy(len,&lBe,2);
           memcpy(&dest[counter], &len[1], 1);
           counter += 1;
           if(a->object[i].type!=2 && a->object[i].type!=5) {
               memcpy(&dest[counter], a->object[i].data, a->object[i].size);
               counter += a->object[i].size;
           }
       }
    }

    // Return number of bytes serialized
    *count = counter;
    dest[counter+1]='\0';

    // success
    return 0;
}


int32_t parserV1(const unsigned char *src,  tlv_chain *list, uint16_t length,int sockfd,char *ips,uint16_t ports)
{
    if(list == NULL || src == NULL)
        return -1;

    // we want an empty chain of tlv
    if(list->used != 0)
        return -1;

    int16_t counter = 0;
  //  printf("\nparser la and length =%d\n",length);
    while(counter < length)
    {
        if(list->used == MAX_TLV_OBJECTS)
            return -1;
        // deserialize type
        list->object[list->used].type = src[counter];
     //   printf("\n type V1=%d",list->object[list->used].type);

        counter++;

        if(list->object[list->used].type!=0) {
            // deserialize size
            memcpy(&list->object[list->used].size, &src[counter], 1);
           // printf("\n tailleV1=%d",list->object[list->used].size);
            counter += 1;
            if((length-counter)<list->object[list->used].size){
                char *msg=" TLV déborder du paquet";
                sendWarning(msg,sockfd,ips,ports);
                return 0;
            }
            if(list->object[list->used].type!=2 && list->object[list->used].type!=5 ) {

                // deserialize data itself, only if data is not NULL
                if (list->object[list->used].size > 0) {
                    list->object[list->used].data = malloc(list->object[list->used].size);
                    memcpy(list->object[list->used].data, &src[counter], list->object[list->used].size);
                   // printf("\n dataV1=%s",list->object[list->used].data);

                    counter += list->object[list->used].size;
                } else {
                    list->object[list->used].data = NULL;
                }
            } else
                list->object[list->used].data = NULL;
        }else{
            list->object[list->used].size = 0;
            list->object[list->used].data = NULL;
        }

        // increase number of tlv objects reconstructed
        list->used++;
    }

    // success
    return 0;

}

///----------------------------------------------------------------------------

char *Hash(char *data){
    unsigned char *d = SHA256(data, strlen(data), 0);
    char *res=malloc(16*sizeof(char));
    memcpy(res,d,16);
    return res;
}
char *concatTriplet(Triplet *d){
    uint16_t seq=htons(d->numDeSeq);
    int len=strlen(d->data)+10;
    char *data=malloc(sizeof(char)*len);
    memcpy(data,d->id,8);
    memcpy(&data[8],&seq,2);
    memcpy(&data[10],d->data,strlen(d->data));
    return data;
}
void afficherdata(Data *datalist){
    Triplet *tmp=datalist->tete;
    printf("\n _________________________________________________________________________________________ ");
    printf("\n Affichage DATA : \n");

    while (tmp!=NULL){
        for (int i = 0; i <8 ; i++) {
            printf("%02x",tmp->id[i]);
        }
        printf("  : %s \n",tmp->data);
        tmp=tmp->suivant;
    }
    printf("\n _________________________________________________________________________________________ \n");
}


Triplet *Getdataintable(Data *datalist,char id[8]){
    Triplet *tmp=datalist->tete;
    Triplet *res=NULL;
    while (tmp!=NULL){

        if(memcmp(tmp->id,id,8)==0){
           // printf("\n [%s] [%s] cmp %d",id ,tmp->id,memcmp(id,tmp->id,8));
            return tmp;
        }
        tmp=tmp->suivant;
    }
    return res;

}

void supprimerData(Data *datalist,char *id){
    Triplet *tmp=datalist->tete;
    Triplet *pres=NULL;
    int count=0;
    while(tmp!=NULL && count==0) {
        if (strcmp(id, tmp->id) == 0) {
            count = 1;
        } else {
            pres = tmp;
            tmp = tmp->suivant;
        }
    }
    if(pres==NULL){
        datalist->tete=datalist->tete->suivant;
    } else if (tmp!=NULL){
        pres->suivant=tmp->suivant;
    }
    return;
}

void NodeState(Data *datalist,char *node,int len,char * ips,uint16_t ports,int sockfd){
    char *id=malloc(sizeof(char)*8);
    memcpy(id,node,8);
    uint16_t seq;
    memcpy(&seq,&node[8],2);
    seq=ntohs(seq);
    char *h=malloc(sizeof(char)*16);
    memcpy(h,&node[10],16);
    len=len-26;
    char *data=malloc(sizeof(char)*len);
    memcpy(data,&node[26],len);
    int num=pow(2,16);
    Triplet *d=Getdataintable(datalist,id);
    if(d!=NULL){
       // printf("\n mama data=%s",d->data);
        if (memcmp(h,Hash(concatTriplet(d)),16)!=0){
         if(memcmp(id,MY_ID,8)==0){// cas 1
             if((seq-d->numDeSeq)%num<32768){
                 d->numDeSeq=(seq+1)%num;
                 d->incr=1;
             }
         }else{// cas 2
             if(((seq-d->numDeSeq)%num)<32768){
                 d->numDeSeq=(seq+1)%num;
                 d->incr=1;
                 supprimerData(datalist,id);
                 insererData(datalist,id,seq,d->data);
             }
         }

        }else{
            char *msg="Inconsistent hash in node state";
            sendWarning(msg,sockfd,ips,ports);
       /// rien a faire
        }
    }else{
        insererData(datalist,id,seq,data);
    }


}
int hex2dec(char hexVal[]) 
{    
    int len = strlen(hexVal); 
      
    // Initializing base value to 1, i.e 16^0 
    int base = 1; 
      
    int dec_val = 0; 
      
    // Extracting characters as digits from last character 
    for (int i=len-1; i>=0; i--) 
    {    
        // if character lies in '0'-'9', converting  
        // it to integral 0-9 by subtracting 48 from 
        // ASCII value. 
        if (hexVal[i]>='0' && hexVal[i]<='9') 
        { 
            dec_val += (hexVal[i] - 48)*base; 
                  
            // incrementing base by power 
            base = base * 16; 
        } 
  
        // if character lies in 'A'-'F' , converting  
        // it to integral 10 - 15 by subtracting 55  
        // from ASCII value 
        else if (hexVal[i]>='a' && hexVal[i]<='f') 
        { 
            dec_val += (hexVal[i] - 55)*base; 
          
            // incrementing base by power 
            base = base*16; 
        } 
    } 
      
    return dec_val; 
} 

int nombreChiffres ( int nombre )
{
	int i = 1;
	if (nombre < 0)
	{
		nombre = -nombre;
	}
	while (nombre >= 10)
	{
		nombre /= 10;
		i++;
	}
/* 
 * Avec une boucle for
 * 	for (i=1; nombre >= 10; nombre /= 10, i++) ;
 */
	return i;
}
unsigned char* parseIp(unsigned char* ipHex)
{
    // for (int i = 0; i <16 ; i++) {
    //     printf(" %02x",ipHex[i]);
    //     }

    // printf("\n");
    unsigned char* ipRes=malloc(45*sizeof(char));
    unsigned char* initMapped="00000000000000000000ffff";
    unsigned char* initOurIp=malloc(12);
    char cmp[24],cmp2[32];
    int j=0;

    for (int i = 0; i <16 ; i++) {

        if (i < 12 && j < 24  )
        {
          initOurIp[i]=ipHex[i];
          sprintf(&cmp[j],"%02x",initOurIp[i]);
          j+=2;

        }


        
           
    }
        
    
        if(strcmp(cmp,initMapped)==0)
        {
            j=7;
            memcpy(ipRes,"::ffff:",7);
            for (size_t i = 12; i < 16; i++)
            {
               int k=ipHex[i];
               if(i==15)
               {
                  sprintf(&ipRes[j],"%d",k);
                  j+=nombreChiffres(k);

               }
               else
               {
                  sprintf(&ipRes[j],"%d.",k);
                  j+=nombreChiffres(k)+1;
               }
            }
            return ipRes;

            // for (size_t i = 12,j=0; i < 16,j<8 ; i++,j+=2)
            // {
            //    sprintf(&ipRes[i],"%d.",ipHex[i]);
            
            // }
            
        }
        else{
            j=0;
            unsigned char* h=malloc(4);
            for (size_t i = 0; i < 16; i+=2)
            {
              
               if(i==14)
               {
                  sprintf(&ipRes[j],"%02x%02x",ipHex[i],ipHex[i+1]);
                  j+=4;

               }
               else
               {
                  sprintf(&ipRes[j],"%02x%02x:",ipHex[i],ipHex[i+1]);
                  j+=5;
               }
            }

              return ipRes;
        }

        
        
        // if ipv6 or ipv4 mapped !

    return ipRes;
}
void sendWarning(char *msg,int sockfd,char * ips,uint16_t ports){
    SA servaddr;
    servaddr.sin6_family = AF_INET6;
   // uint16_t port=ntohs(addr->sin6_port);
    char ip[INET6_ADDRSTRLEN];
    //inet_ntop(AF_INET6,&addr->sin6_addr,ip,sizeof(ip));
    servaddr.sin6_port = htons(ports);
    int p1=inet_pton(AF_INET6,ips,&servaddr.sin6_addr);
    if(p1==-1)
    {
        perror(" ip err ");
    }
    tlv_chain chaine;
    unsigned char chainbuff[1024]={0} ;
    uint16_t l = 0;
    memset(&chaine, 0, sizeof(chaine));
    add_tlv(&chaine,WARNING,strlen(msg),msg);
    tlv_chain_toBuff(&chaine, chainbuff, &l);
    char *paquet=chain2Paquet(chainbuff,l);
    if (sendto(sockfd,(const char *)paquet,l+4,MSG_CONFIRM,(const struct sockaddr *)&servaddr,sizeof(servaddr))>0)
        printf("\n paquet type 9 sent  \n");
    else printf("\n error , paquet type 9 non sent  \n");



}

void sendSerieTlvNode(Data *datalist,int sockfd,char *ips,uint16_t ports){
    Triplet *tmp=datalist->tete;
    SA servaddr;
    servaddr.sin6_family = AF_INET6;

    int val=1;
   // int poly_port=setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&val,sizeof(val));


    val=0;
    //int poly=setsockopt(sockfd,IPPROTO_IPV6,IPV6_V6ONLY,&val,sizeof(val));
    //uint16_t port=ntohs(addr->sin6_port);
    char ip[INET6_ADDRSTRLEN];
    //inet_ntop(AF_INET6,&addr->sin6_addr,ip,sizeof(ip));
    servaddr.sin6_port = htons(ports);
    int p1=inet_pton(AF_INET6,ips,&servaddr.sin6_addr);
    if(p1==-1)
    {
        perror(" ip err ");
    }




    tlv_chain chaine;
    memset(&chaine, 0, sizeof(chaine));
    char *h;
    char *data=malloc(sizeof(char)*26);
    unsigned char chainbuff[99000]={0} ;
    uint16_t l = 0;
    int len;
   // printf("\n port %d",addr->sin6_port);
    while (tmp!=NULL){
        h=Hash(concatTriplet(tmp));
        uint16_t seq=htons(tmp->numDeSeq);
        memcpy(data,tmp->id,8);
        memcpy(&data[8],&seq,2);
        memcpy(&data[10],h,16);
        add_tlv(&chaine,NODE_HASH,26,data);
        tmp=tmp->suivant;
    }
    tlv_chain_toBuff(&chaine, chainbuff, &l);
    char *paquet=chain2Paquet(chainbuff,l);
   // printf("\n the L is %d \n",l);

    if(sendto(sockfd,(const char *)paquet,l+4,MSG_CONFIRM,(const struct sockaddr *)&servaddr,sizeof(servaddr))>0)
        printf("\n serie TLV Node Hash 6 sent  \n");
    else
        printf("\n error , serie TLV Node Hash 6 non sent  \n");
}

void parserTLV(Data *datalist,Voisins *voisins,tlv_chain *list,int index,char * ips,uint16_t ports,int sockfd){
    int length;
    char *body;
    char *data;
    char* paquet;
    unsigned char *id;
    char *h;
    int len;
    int p1;
    int val;
    int poly_port;
    int poly;
    uint16_t seq;
    SA servaddr;
    servaddr.sin6_family = AF_INET6;
    unsigned char chainbuff[1024]={0} ;
    uint16_t l = 0;
    switch (list->object[index].type){
        case PAD_1:
            printf("\n #0 [PAD_1] received \n");
            break;
        case PAD_N:
            printf("\n #1 [PAD_N] received \n");
            break;
        case NEIGH_R:
            printf("\n #2 [NEIGH_R] received \n");
            //Ce TLV demande au récepteur d’envoyer un TLVNeighbour
            Voisin *v=hasardVoisin(voisins);
            servaddr.sin6_port = htons(v->port);
            int p;
            memcpy(&servaddr.sin6_addr,v->ip,16);
            tlv_chain neigh;
            memset(&neigh, 0, sizeof(neigh));
            data=malloc(strlen(v->ip)+2);
            memcpy(data,v->ip,strlen(v->ip));
            p=v->port;
            short p2=htons(p);
            memcpy(&data[strlen(v->ip)],&p2,2);
            add_tlv(&neigh,NEIGH,strlen(data),data);
            tlv_chain_toBuff(&neigh, chainbuff, &l);
            paquet=chain2Paquet(chainbuff,l);
            if (sendto(sockfd,(const char *)paquet,l+4,MSG_CONFIRM,(const struct sockaddr *)&servaddr,sizeof(servaddr))>0)
             printf("\n paquet type 3 sent  \n");
            else printf("\n error , paquet type 3 non sent  \n");
            break;
        case NEIGH:
            printf("\n #3 [NEIGH] received \n");
            tlv_chain netHash;
            memset(&netHash, 0, sizeof(netHash));
          //  printf("\n ip=%s",list->object[index].data);
            //Ce TLV contient l’adresse d’un voisin vivant de l’émetteur
            data=list->object[index].data;
             int8_t length=list->object[index].size;
           // printf("\n len=%d",l);
            uint16_t port=0;
            unsigned char *ip=malloc(16*sizeof(char));
            memcpy(ip,data,length-2);
            memcpy(&port,&data[length-2],2);
            short port2=ntohs(port);
           // printf("\n port =%d ",port);
           // printf("\n ip formatted : %s\n",parseIp(ip));
            servaddr.sin6_family = AF_INET6;
             servaddr.sin6_port = htons(port2);
           //  printf("\n djelid ip:%s",parseIp(ip));
            memcpy(&servaddr.sin6_addr,ip,16);
           //  p1=inet_pton(AF_INET6,parseIp(ip),&servaddr.sin6_addr);
            char *net=NetworkHash(datalist);
            add_tlv(&netHash,NET_HASH,16,net);
            tlv_chain_toBuff(&netHash,chainbuff, &l);
            paquet=chain2Paquet(chainbuff,l);
            if(sendto(sockfd,(const char *)paquet,l+4,MSG_CONFIRM,(const struct sockaddr *)&servaddr,sizeof(servaddr))>0)
            {
                    printf("\n paquet type 4 sent  \n");
            }
            break;
        case NET_HASH:
            printf("\n #4 [NET_HASH] received \n");
            ip[INET6_ADDRSTRLEN];
            servaddr.sin6_port = htons(ports);
            p=inet_pton(AF_INET6,ips,&servaddr.sin6_addr);
            if(p==-1)
            {
                perror(" ip err ");
            }
            char *myHash=NetworkHash(datalist);
            if(strcmp(myHash,list->object[index].data)!=0){
                tlv_chain netstate;
                memset(&netstate, 0, sizeof(netstate));
                add_tlv(&netstate,NET_STATE_R,0,NULL);
                tlv_chain_toBuff(&netstate, chainbuff, &l);
                paquet=chain2Paquet(chainbuff,l);
                if(sendto(sockfd,(const char *)paquet,l+4,MSG_CONFIRM,(const struct sockaddr *)&servaddr,sizeof(servaddr))>0)
                  printf("\n paquet type 5 sent  \n");
                else
                    printf("\n error , paquet type 5 non sent  \n");

            }

            break;
        case NET_STATE_R:
            printf("\n #5 [NET_STATE_R] received \n");
            //Ce TLV demande au récepteur d’envoyer une série de TLVNode Hash
            sendSerieTlvNode(datalist,sockfd,ips,ports);
            break;
        case NODE_HASH:
            //Ce TLV est envoyé en réponse à un TLVNetwork State Request.
            printf("\n #6 [NODE_HASH] received \n");
            servaddr.sin6_port = htons(ports);
            p=inet_pton(AF_INET6,ips,&servaddr.sin6_addr);
            if(p==-1)
            {
                perror(" ip err ");
            }
            id=malloc(sizeof(char)*8);
            memcpy(id,list->object[index].data,8);
            memcpy(&seq,&(list->object[index].data[8]),2);
            seq=ntohs(seq);
            h=malloc(sizeof(char)*16);
            memcpy(h,&(list->object[index].data[10]),16);

            tlv_chain netstatereq;
            memset(&netstatereq, 0, sizeof(netstatereq));
           Triplet *d=Getdataintable(datalist,id);
            if(d==NULL){
                add_tlv(&netstatereq,NODE_STATE_R,8,id);
                tlv_chain_toBuff(&netstatereq, chainbuff, &l);
                paquet=chain2Paquet(chainbuff,l);
               if(sendto(sockfd,(const char *)paquet,l+4,MSG_CONFIRM,(const struct sockaddr *)&servaddr,sizeof(servaddr))>0)
                    printf("\n paquet type 7 sent ! \n");
                else  printf("\n error , paquet type 7 non sent  \n");
            } else if(d->incr==1){
                d->incr=0;
            //else if (memcmp(h,Hash(concatTriplet(d)),8)!=0){
                add_tlv(&netstatereq,NODE_STATE_R,8,id);
                tlv_chain_toBuff(&netstatereq, chainbuff, &l);
                paquet=chain2Paquet(chainbuff,l);
                if(sendto(sockfd,(const char *)paquet,l+4,MSG_CONFIRM,(const struct sockaddr *)&servaddr,sizeof(servaddr))>0)
                 printf("\n paquet type 7 sent  \n");
                else  printf("\n error , paquet type 7 non sent  \n");
            } else {
                /// rien a faire
            }
            break;
        case NODE_STATE_R:
            printf("\n #7 [NODE_STATE_R] received \n");
            //Ce TLV demande au récepteur d’envoyer un TLVNode Statedécrivant l’état du nœud indiquépar le champNode Id
            char *idnode=list->object[index].data;
            //port=ntohs(addr->sin6_port);
            // char *ip=inet_ntop(addr->sin6_addr);
            ip[INET6_ADDRSTRLEN];
            //inet_ntop(AF_INET6,&addr->sin6_addr,ip,sizeof(ip));
            servaddr.sin6_port = htons(ports);
            p=inet_pton(AF_INET6,ips,&servaddr.sin6_addr);
            if(p==-1)
            {
                perror(" ip err ");
            }
            tlv_chain node_state;
            memset(&node_state, 0, sizeof(node_state));
            Triplet *d1=Getdataintable(datalist,idnode);
            if (d1!=NULL) {
                int tailletosend=26 + strlen(d1->data);
                char *toSend = malloc(tailletosend*sizeof(char));
                memcpy(toSend, idnode, 8);
                uint16_t seqno =htons(d1->numDeSeq);
                memcpy(&toSend[8], &seqno, 2);
                char *nHash = Hash(concatTriplet(d1));
              //  printf("\nHAsh :%s",nHash);
                memcpy(&toSend[10], nHash, 16);
                memcpy(&toSend[26], d1->data, strlen(d1->data));
                //printf("\nla taille de tosend est :%d",strlen(toSend));
                add_tlv(&node_state, NODE_STATE, tailletosend, toSend);
                tlv_chain_toBuff(&node_state, chainbuff, &l);
                paquet = chain2Paquet(chainbuff,l);
                //printf("\nla taille l est:%d",l);
                if (sendto(sockfd, (const char *) paquet,l + 4, 0, (const struct sockaddr *) &servaddr, sizeof(servaddr)) > 0)
                    printf("\n paquet type 8 sent  \n");
                else printf("\n error , paquet type 8 non sent  \n");
            } else printf("\n data non trouve  \n");


            break;
        case NODE_STATE:
            printf("\n #8 [NODE_STATE] received \n");
             // Ce TLV est envoyé en réponse à un TLVNode State Request
             NodeState(datalist,list->object[index].data,list->object[index].size,ips,ports,sockfd);

            break;
        case WARNING:
            printf("\n #9 [WARNING] received \n");
            printf("- %s \n",list->object[index].data);
            break;
        default:
            printf("default type");
    }

}


char* chain2Paquet (char *chain,uint16_t  len)
{

    char *res=malloc(sizeof(char)*PAQ_SIZE);
    short lBe=htons(len);
    res[0]=0b01011111;
    res[1]=1;
    memcpy(res+2,&lBe,2);
    if(len>1020){
        memcpy(res+4,chain,1020);
    }else memcpy(res+4,chain,len);

    return res;

}

void parserPaquet(Data *datalist,Voisins *voisins,char *buf,SA *addr,int sockfd,int lenp){
    int index=0;
    tlv_chain list;
    memset(&list, 0, sizeof(list));
    uint16_t len;
    char taile[2];

    if(buf[0]==95 && buf[1]==1) {
        uint16_t port = ntohs(addr->sin6_port);
        // char *ip=inet_ntop(addr->sin6_addr);
        char ip[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &addr->sin6_addr, ip, sizeof(ip));
        if (rechercheEmetteur(voisins, addr, port) == 0 && voisins->used == 15) {
            printf(" \nerror ---- : le paquet est ignoré \n");
            return;
        }
       // printf("\n\nin the beg  ip =%s  and port =%d ", ip, port);
       // printf("\n\nthe nb of voisins =%d ", nbVoisin(voisins));
       // printf("\n\nthe length of paquet send =%d ", lenp);

        miseAjourVoisins(voisins, addr, port);
        memcpy(&len, &buf[2], 2);
        len = ntohs(len);
       // printf("\n\nthe length of paquet in=%d ", len);
      // printf("\n \n lenpa=%d and lenin=%d",lenp,len);
        int paquettaille = len + 4;
        if ( lenp >paquettaille) {
          char msg[45];
          sprintf(msg,"the paquet size is larger then %d",paquettaille);
          sendWarning(msg,sockfd,ip,port);
        } else {
            parserV1(buf + 4, &list, len,sockfd,ip,port);
            //printf("\nused=%d\n", list.used);
            while (index < list.used) {

                parserTLV(datalist, voisins, &list, index, ip,port, sockfd);
                index++;
            }

        }
    }
        else
        {
            printf(" error ---- : paquet non reconnu");
        }



}

int rechercheEmetteur(Voisins *voisins,SA *addr, uint16_t port){
    int count=0;
    while(count<Max_voisin){
        if(voisins->TableDevoisins[count]!=NULL){

            if(voisins->TableDevoisins[count]->port==port && memcmp(voisins->TableDevoisins[count]->ip,&addr->sin6_addr,16)==0){
                return 1;
            }
        }
        count++;
    }
    return 0;
}
void addVoisin(Voisins *voisins,SA *addr, uint16_t port){
    struct timespec now;
    int rc=clock_gettime(CLOCK_REALTIME,&now);
    if(rc<0)
    {
        perror("erreur geettime");
        exit(EXIT_FAILURE);
    }
    int count=0;
    while(count<Max_voisin){
        if(voisins->TableDevoisins[count]==NULL){
            voisins->TableDevoisins[count]=malloc(sizeof(Voisin));
            voisins->TableDevoisins[count]->port=port;
            voisins->TableDevoisins[count]->ip=malloc(45);
            memcpy(voisins->TableDevoisins[count]->ip,&addr->sin6_addr,16);
            voisins->TableDevoisins[count]->date=now;
            voisins->TableDevoisins[count]->permanent=0;
            voisins->used+=1;
            return;
        }
        count++;
    }
}
void affichervoisins(Voisins *voisins){
    printf("\n _________________________________________________________________________________________ ");
    printf("\n Affichage VOISINS : \n");
    int count=0;
    while(count<Max_voisin){
        if(voisins->TableDevoisins[count]!=NULL){
            printf("\n port =%d || IP = %s",voisins->TableDevoisins[count]->port,parseIp(voisins->TableDevoisins[count]->ip));
        }
        count++;
    }

    printf("\n _________________________________________________________________________________________ \n");

}
void modifierVoisin(Voisins *voisins,SA *addr, uint16_t port){
    struct timespec now;
    int rc=clock_gettime(CLOCK_REALTIME,&now);
    if(rc<0)
    {
        perror("erreur geettime");
        exit(EXIT_FAILURE);
    }
    int count=0;
    while(count<Max_voisin){
        if(voisins->TableDevoisins[count]!=NULL){
            if(voisins->TableDevoisins[count]->port==port && memcmp(voisins->TableDevoisins[count]->ip,&addr->sin6_addr,16)==0){
               // printf("\nmodifier la data\n");
                voisins->TableDevoisins[count]->date=now;
            }
        }
        count++;
    }
}
void miseAjourVoisins(Voisins *voisins,SA *addr, uint16_t port){
    if(rechercheEmetteur(voisins,addr,port)==1){
        modifierVoisin(voisins,addr,port);
    }else{
        addVoisin(voisins,addr,port);
    }
}
int nbVoisin(Voisins *voisins){
    int count=0;
    int cpt=0;
    while(count<Max_voisin){
        if(voisins->TableDevoisins[count]!=NULL){
            cpt++;
        }
        count++;
    }
    return cpt;
}
void TLVNetworkHash(Voisin *v,Data *datalist,int sockfd){
    SA servaddr;
    unsigned char chainbuff[1024]={0} ;
    uint16_t l = 0;
    int taille=16;
    servaddr.sin6_family = AF_INET6;
    servaddr.sin6_port = htons(v->port);
    memcpy(&servaddr.sin6_addr,v->ip,16);
    tlv_chain neigh;
    memset(&neigh, 0, sizeof(neigh));
    char *net=NetworkHash(datalist);
    add_tlv(&neigh,NET_HASH,16,net);
    tlv_chain_toBuff(&neigh,chainbuff, &l);
    char *paquet=chain2Paquet(chainbuff,l);
    if(sendto(sockfd,(const char *)paquet,l+4,MSG_CONFIRM,(const struct sockaddr *)&servaddr,sizeof(servaddr))>0)
    {
        printf("\n paquet type 4 sent  \n");
    }
    else{
        printf("\n erorr , type 4 non sent");
    }



}
void sendNetHAsh(Voisins *voisins,Data *datalist,int sockfd){
    int count=0;
    while(count<Max_voisin){
        if(voisins->TableDevoisins[count]!=NULL){
            TLVNetworkHash(voisins->TableDevoisins[count],datalist,sockfd);
        }
        count++;
    }
}
Voisin *hasardVoisin(Voisins *voisins){
    int nbgen=rand()%voisins->used;
    int count=0;
    while(count<Max_voisin){
        if(voisins->TableDevoisins[count]!=NULL){
            if(count==nbgen){
                return voisins->TableDevoisins[count];
            }
        }
        count++;
    }

}
void moinsde5voisins(Voisins *voisins,int sockfd){
    if (voisins->used==0) return;
    if (voisins->used<5){
       // printf("\nthe number of used %d\n",voisins->used);
        SA servaddr;
        Voisin *v=hasardVoisin(voisins);

        servaddr.sin6_family = AF_INET6;
        servaddr.sin6_port = htons(v->port);
        int p;
        memcpy(&servaddr.sin6_addr,v->ip,16);
        //p=inet_pton(AF_INET6,v->ip,&servaddr.sin6_addr);
        //if(p==-1)
        //{
          //  perror(" ip err ");
        //}
        int n, len;
        tlv_chain chain1, chain2;
        memset(&chain1, 0, sizeof(chain1));
        memset(&chain2, 0, sizeof(chain2));
        unsigned char chainbuff[1024]={0} ;
        uint16_t l = 0;
        add_tlv(&chain1,NEIGH_R,0,NULL);
        tlv_chain_toBuff(&chain1, chainbuff, &l);
        char* paquet=chain2Paquet(chainbuff,l);

        if(sendto(sockfd, (char *)paquet, sizeof(paquet),0, (const struct sockaddr *) &servaddr,sizeof(servaddr))>0)
        {
            printf("paquet 2 (NEIGH_REQUEST) sent.\n");
        
        }
}
}

void *sendNet20s(void *args){
    arg2 *argss=(arg2 *)args;
    while (1){
       // printf("\n datalist used  %d",argss->datalist->used);
        sendNetHAsh(argss->arg1,argss->datalist,argss->sockfd);
        afficherdata(argss->datalist);
        affichervoisins(argss->arg1);
        sleep(20);
    }
    return NULL;
}

void *miseAjour20s(void *args){
    arg *argss = (arg *)args;
    while (1){
       // printf("\n on est la  and the num is %d",argss->arg1->used);
        parcoursVoisins(argss->arg1);
        moinsde5voisins(argss->arg1,argss->sockfd);
        sleep(20);
    }
   return NULL;
}

void parcoursVoisins(Voisins *voisins){

struct timespec now;
int rc=clock_gettime(CLOCK_REALTIME,&now);
if(rc<0)
{
    perror("erreur geettime");
    exit(EXIT_FAILURE);
}


    int count=0;
    while(count<Max_voisin){
        if(voisins->TableDevoisins[count]!=NULL){
            if(now.tv_sec-voisins->TableDevoisins[count]->date.tv_sec>70 && voisins->TableDevoisins[count]->permanent==0){
                voisins->TableDevoisins[count]=NULL;
                voisins->used-=1;
            }
        }
        count++;
    }
}


char *NetworkHash(Data *datalist){
    Triplet *tmp=datalist->tete;
    char *hashi;
    int i=0;
    char *hashNet=malloc(16*datalist->used);
    while (tmp!=NULL){
        hashi=Hash(concatTriplet(tmp));
        memcpy(&hashNet[i],hashi,16);
        i+=16;
        tmp=tmp->suivant;
    }
    return Hash(hashNet);
}
void insererData(Data *datalist,char *id,uint16_t seq,char *donnee){
    Triplet *t=malloc(sizeof(Triplet));
    t->numDeSeq=seq;
    t->incr=0;
    //memcpy(t->numDeSeq,seq,2);
    memcpy(t->id,id,8);
    t->data=malloc(strlen(donnee));
    memcpy(t->data,donnee,strlen(donnee));
    t->suivant=NULL;
    Triplet *tmp=datalist->tete;
    Triplet *pres=NULL;
    if(tmp==NULL){
        datalist->tete=t;
        datalist->used=1;
        return;
    }
    int cnt=1;
    while (tmp!=NULL && cnt==1){
        if(memcmp(tmp->id,id,8)>0){
            cnt=0;
        }else {
            pres = tmp;
            tmp = tmp->suivant;
        }
    }
    if(tmp==NULL){
        pres->suivant=t;
    }else if(pres==NULL){
        t->suivant=tmp;
        datalist->tete=t;
    } else{
        pres->suivant=t;
        t->suivant=tmp;
    }
    datalist->used+=1;
}

void initaddr(Voisins *voisins,char *hostname,uint16_t port){
    struct addrinfo* result;
    struct addrinfo* res;
    int error;
    void *addr;
    SA sinaddr;
    char ipver;
    char ipstr[INET6_ADDRSTRLEN];
    char *tmp=malloc(sizeof(char)*60);
    char *f="::ffff:";
    memset(&sinaddr, 0, sizeof(sinaddr));

    error = getaddrinfo(hostname, NULL, NULL, &result);
    if (error != 0) {
        if (error == EAI_SYSTEM) {
            perror("getaddrinfo");
        } else {
            fprintf(stderr, "error in getaddrinfo: %s\n", gai_strerror(error));
        }
        exit(EXIT_FAILURE);
    }

    for (res = result; res != NULL; res = res->ai_next) {
        char hostname[NI_MAXHOST];
        // Identification de l'adresse courante
        if (res->ai_family == AF_INET) { // IPv4
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = '4';
            inet_ntop(res->ai_family, addr, ipstr, sizeof(ipstr));
            memcpy(tmp,f,strlen(f));
            memcpy(&tmp[strlen(f)],ipstr,strlen(ipstr));
            int p;
            p=inet_pton(AF_INET6,tmp,addr);
            if(p==-1)
            {
                perror(" ip err ");
                exit(1);
            }
            memcpy(&sinaddr.sin6_addr,addr,16);
            if(rechercheEmetteur(voisins,&sinaddr,port)==0)
            {
                addVoisin(voisins,&sinaddr,port);
            }

           // printf(" tmp %s\n", tmp);

        }
        else { // IPv6
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)res->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = '6';
            inet_ntop(res->ai_family, addr, ipstr, sizeof(ipstr));
            memcpy(&sinaddr.sin6_addr,&(ipv6->sin6_addr),16);
           if(rechercheEmetteur(voisins,&sinaddr,port)==0){
                addVoisin(voisins,&sinaddr,port);
            }
        }

    }

    freeaddrinfo(result);

}


